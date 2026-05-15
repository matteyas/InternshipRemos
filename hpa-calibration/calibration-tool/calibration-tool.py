"""
Sequencer Calibration Tool
==========================
Single-file tkinter application for fitting and storing HPA power-model
coefficients.

Structure:
    Theme                   - all visual constants
    PowerModel              - fitting / prediction math
    CalibrationResult       - fit output (dataclass)
    *Config dataclasses     - typed config DTOs with JSON serialisation
    make_*                  - widget factory helpers
    VWFGroup                - one measurement row (voltage, watts, frequency)
    HPAPanel                - HPA calibration input panel
    PredictorPanel          - live (voltage, frequency) -> watt prediction widget
    SystemConfigPanel       - left-column system / channel / coefficient display
    SequencerCalibrationApp - top-level orchestrator
"""

#=========#
# Imports #
#=========#
from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from typing import Callable

import numpy as np
from scipy.optimize import curve_fit
import tkinter as tk
from tkinter import filedialog, messagebox, ttk


#==================#
# Input Validation #
#==================#
class RangeValidation:
    """Input ranges for voltages and watts"""

    # voltage
    voltage_lower_bound = 0
    voltage_upper_bound = 10
    
    # watt
    watt_lower_bound    = 0
    watt_upper_bound    = 200


#==========#
# Platform #
#==========#
if sys.platform == "win32":
    import ctypes
    try:
        ctypes.windll.user32.SetProcessDPIAware()
    except AttributeError:
        pass


#=======#
# Theme #
#=======#
class Theme:
    """GUI fonts / colors"""

    ### palette
    BG       = "#1e1e2e"
    SURFACE  = "#2a2a3e"
    CARD     = "#313148"
    BORDER   = "#44445a"
    ACCENT   = "#7c6af7"
    ACCENT2  = "#56b6c2"
    SUCCESS  = "#50fa7b"
    WARN     = "#ffb86c"
    ERROR    = "#ff5555"
    TEXT     = "#cdd6f4"
    MUTED    = "#6c7086"
    ENTRY_BG = "#1a1a2e"
    HPA1     = "#7c6af7"
    HPA2     = "#56b6c2"

    ### fonts
    FACE       = ("Segoe UI", "Ubuntu", "Liberation Sans", "Arial")
    SMALL      = (FACE,  8)
    MID        = (FACE,  9)
    LARGE      = (FACE, 10)
    SMALL_BOLD = (FACE,  8, "bold")
    MID_BOLD   = (FACE,  9, "bold")
    LARGE_BOLD = (FACE, 10, "bold")

    ### helper method
    @staticmethod
    def lighten(hex_color: str) -> str:
        """Return a slightly lighter version of a hex colour."""
        r = min(255, int(hex_color[1:3], 16) + 30)
        g = min(255, int(hex_color[3:5], 16) + 30)
        b = min(255, int(hex_color[5:7], 16) + 30)
        return f"#{r:02x}{g:02x}{b:02x}"


#==============#
# Domain model #
#==============#

# Ordered coefficient names; order must match _poly_coeffs / curve_fit params.
COEFF_KEYS: tuple[str, ...] = ("a0", "a1", "a2", "b0", "b1", "b2", "c0", "c1", "c2")


@dataclass(frozen=True)
class CalibrationResult:
    """Output dataclass from PowerModel.fit()"""
    a0:   float = 0.0
    a1:   float = 0.0
    a2:   float = 0.0
    b0:   float = 0.0
    b1:   float = 0.0
    b2:   float = 0.0
    c0:   float = 0.0
    c1:   float = 0.0
    c2:   float = 0.0
    fmin: float = 0.0
    fmax: float = 0.0
    rmse: float = 0.0
    r2:   float = 0.0

    def coeff_tuple(self) -> tuple[float, ...]:
        return tuple(getattr(self, k) for k in COEFF_KEYS)


class PowerModel:
    """Biquadratic model for fitting and estimating power based on voltage and frequency."""

    @staticmethod
    def _poly_coeffs(
        fn: float | np.ndarray,
        a0: float, a1: float, a2: float,
        b0: float, b1: float, b2: float,
        c0: float, c1: float, c2: float,
    ) -> tuple:
        """Quadratic-in-frequency polynomial coefficients a, b, c."""
        fn2 = fn ** 2
        return (
            a0 * fn2 + a1 * fn + a2,
            b0 * fn2 + b1 * fn + b2,
            c0 * fn2 + c1 * fn + c2,
        )

    @staticmethod
    def _evaluate(
        x: tuple[np.ndarray, np.ndarray], *coeffs: float
    ) -> np.ndarray:
        """W = a(fn)·V² + b(fn)·V + c(fn).  Signature compatible with curve_fit."""
        v, fn = x
        a, b, c = PowerModel._poly_coeffs(fn, *coeffs)
        return a * v**2 + b * v + c

    @staticmethod
    def predict(v: float, f: float, result: CalibrationResult) -> float:
        """Predict watts for a single (voltage, frequency) pair."""
        fn = ((f - result.fmin) / (result.fmax - result.fmin)
              if result.fmax > result.fmin else 0.0)
        a, b, c = PowerModel._poly_coeffs(fn, *result.coeff_tuple())
        return a * v**2 + b * v + c

    @staticmethod
    def fit(
        v: np.ndarray,
        w: np.ndarray,
        f: np.ndarray,
    ) -> CalibrationResult:
        """
        Fit the power model to measurement arrays.

        Raises
        ------
        ValueError
            If data are insufficient (< 9 data points or < 3 unique frequencies).
            If voltage or watt are unreasonable (range validation).
        RuntimeError
            If scipy curve_fit fails to converge.
        """
        if len(v) < 9:
            raise ValueError(f"Need at least 9 data points; got {len(v)}.")
        if len(np.unique(f)) < 3:
            raise ValueError(f"Need at least 3 unique frequencies; got {len(np.unique(f))}.")

        v_lower, v_upper = RangeValidation.voltage_lower_bound, RangeValidation.voltage_upper_bound
        if v.min() < v_lower or v.max() > v_upper:
            raise ValueError(f"Make sure voltage values are within {v_lower} - {v_upper}V.")
        
        w_lower, w_upper = RangeValidation.watt_lower_bound, RangeValidation.watt_upper_bound
        if w.min() < w_lower or w.max() > w_upper:
            raise ValueError(f"Make sure watt values are within {w_lower} - {w_upper}W.")

        fmin, fmax = float(f.min()), float(f.max())
        fn = (f - fmin) / (fmax - fmin) if fmax > fmin else np.zeros_like(f)

        try:
            params, _ = curve_fit(
                PowerModel._evaluate, (v, fn), w,
                p0=[1.0] + [0.0] * 8, maxfev=5000,
            )
        except Exception as ex:
            raise RuntimeError(f"Curve fitting failed: {ex}") from ex

        w_pred    = PowerModel._evaluate((v, fn), *params)
        residuals = w - w_pred
        rmse = float(np.sqrt(np.mean(residuals**2)))
        r2   = float(1 - np.sum(residuals**2) / np.sum((w - w.mean())**2))

        return CalibrationResult(
            **dict(zip(COEFF_KEYS, params.tolist())),
            fmin=fmin, fmax=fmax, rmse=rmse, r2=r2,
        )


#====================#
# Config dataclasses #
#====================#
@dataclass
class ChannelConfig:
    label:     str  = ""
    enabled:   bool = True
    part_name: str  = ""


@dataclass
class TempSensorConfig:
    label:     str  = ""
    enabled:   bool = True
    part_name: str  = ""


@dataclass
class PowerCalcConfig:
    enabled: bool  = False
    a0:      float = 0.0
    a1:      float = 0.0
    a2:      float = 0.0
    b0:      float = 0.0
    b1:      float = 0.0
    b2:      float = 0.0
    c0:      float = 0.0
    c1:      float = 0.0
    c2:      float = 0.0
    fmin:    float = 0.0
    fmax:    float = 0.0


@dataclass
class AppConfig:
    """Top-level configuration object.  Owns serialisation to/from JSON dict."""

    system:      str = "Sequencer"
    description: str = "System information for the Sequencer"

    chan0:            ChannelConfig    = field(default_factory=lambda: ChannelConfig(label="Channel 1", part_name="PartA"))
    chan1:            ChannelConfig    = field(default_factory=lambda: ChannelConfig(label="Channel 2", part_name="PartB"))
    temperature1:     TempSensorConfig = field(default_factory=lambda: TempSensorConfig(label="HPA 1", part_name="PartA"))
    temperature2:     TempSensorConfig = field(default_factory=lambda: TempSensorConfig(label="HPA 2", part_name="PartB"))
    powercalculation1: PowerCalcConfig = field(default_factory=PowerCalcConfig)
    powercalculation2: PowerCalcConfig = field(default_factory=PowerCalcConfig)

    ### serialisation
    def to_dict(self) -> dict:
        def _device(obj: ChannelConfig | TempSensorConfig) -> dict:
            return {"label": obj.label, "enabled": obj.enabled, "partName": obj.part_name}

        def _pc(obj: PowerCalcConfig) -> dict:
            return {
                "enabled": obj.enabled,
                **{k: getattr(obj, k) for k in (*COEFF_KEYS, "fmin", "fmax")},
            }

        return {
            "system":            self.system,
            "description":       self.description,
            "chan0":              _device(self.chan0),
            "chan1":              _device(self.chan1),
            "temperature1":      _device(self.temperature1),
            "temperature2":      _device(self.temperature2),
            "powercalculation1": _pc(self.powercalculation1),
            "powercalculation2": _pc(self.powercalculation2),
        }

    @classmethod
    def from_dict(cls, data: dict) -> AppConfig:
        def _channel(d: dict) -> ChannelConfig:
            return ChannelConfig(
                label=d.get("label", ""),
                enabled=d.get("enabled", True),
                part_name=d.get("partName", ""),
            )

        def _temp(d: dict) -> TempSensorConfig:
            return TempSensorConfig(
                label=d.get("label", ""),
                enabled=d.get("enabled", True),
                part_name=d.get("partName", ""),
            )

        def _pc(d: dict) -> PowerCalcConfig:
            return PowerCalcConfig(
                enabled=d.get("enabled", False),
                **{k: float(d.get(k, 0.0)) for k in (*COEFF_KEYS, "fmin", "fmax")},
            )

        return cls(
            system=data.get("system", ""),
            description=data.get("description", ""),
            chan0=_channel(data.get("chan0", {})),
            chan1=_channel(data.get("chan1", {})),
            temperature1=_temp(data.get("temperature1", {})),
            temperature2=_temp(data.get("temperature2", {})),
            powercalculation1=_pc(data.get("powercalculation1", {})),
            powercalculation2=_pc(data.get("powercalculation2", {})),
        )


#================#
# Widget Factory #
#================#
def make_label(
    parent: tk.Widget, text: str, *,
    bold: bool = False, size: int = 9, color: str = Theme.TEXT, **kw,
) -> tk.Label:
    weight = "bold" if bold else "normal"
    bg = kw.pop("bg", parent.cget("bg"))
    return tk.Label(parent, text=text, font=(Theme.FACE, size, weight),
                    fg=color, bg=bg, **kw)


def make_entry(
    parent: tk.Widget, textvariable: tk.Variable | None = None,
    width: int = 8, **kw,
) -> tk.Entry:
    return tk.Entry(
        parent, textvariable=textvariable, width=width,
        bg=Theme.ENTRY_BG, fg=Theme.TEXT, insertbackground=Theme.TEXT,
        relief="flat", highlightthickness=1,
        borderwidth=0,
        highlightcolor=Theme.ACCENT, highlightbackground=Theme.BORDER,
        font=Theme.MID, **kw,
    )


def make_button(
    parent: tk.Widget, text: str, command: Callable, *,
    color: str = Theme.ACCENT, width: int = 18, **kw,
) -> tk.Button:
    btn = tk.Button(
        parent, text=text, command=command,
        bg=color, fg="white", activebackground=color, activeforeground="white",
        relief="flat", font=Theme.SMALL_BOLD, cursor="hand2",
        width=width, pady=5,
        highlightthickness=0,
        borderwidth=0,
        **kw,
    )
    btn.bind("<Enter>", lambda _: btn.config(bg=Theme.lighten(color)))
    btn.bind("<Leave>", lambda _: btn.config(bg=color))
    return btn


def make_card(parent: tk.Widget, **kw) -> tk.Frame:
    return tk.Frame(parent, bg=Theme.CARD, bd=0,
                    highlightthickness=1, highlightbackground=Theme.BORDER, **kw)


def make_section_header(parent: tk.Widget, title: str) -> None:
    row = tk.Frame(parent, bg=Theme.BG)
    row.pack(fill="x", padx=6, pady=(12, 2))
    tk.Label(row, text=title.upper(), bg=Theme.BG, fg=Theme.MUTED,
             font=Theme.SMALL_BOLD).pack(side="left")
    tk.Frame(row, bg=Theme.BORDER, height=1).pack(
        side="left", fill="x", expand=True, padx=6, pady=6,
    )


def make_field_row(
    parent: tk.Widget, label: str,
    var: tk.Variable, *, readonly: bool = False,
) -> tk.Entry:
    row = tk.Frame(parent, bg=Theme.CARD)
    row.pack(fill="x", padx=8, pady=2)
    tk.Label(row, text=label, bg=Theme.CARD, fg=Theme.MUTED,
             font=Theme.MID, width=14, anchor="w").pack(side="left")
    entry = make_entry(row, textvariable=var, width=12)
    if readonly:
        entry.config(state="readonly", fg=Theme.MUTED)
    entry.pack(side="left", padx=4, pady=3)
    return entry


def make_check_row(parent: tk.Widget, label: str, var: tk.BooleanVar) -> None:
    row = tk.Frame(parent, bg=Theme.CARD)
    row.pack(fill="x", padx=8, pady=2)
    tk.Label(row, text=label, bg=Theme.CARD, fg=Theme.MUTED,
             font=Theme.MID, width=14, anchor="w").pack(side="left")
    tk.Checkbutton(row, variable=var, bg=Theme.CARD, fg=Theme.TEXT,
                   activebackground=Theme.CARD, selectcolor=Theme.ENTRY_BG,
                   highlightthickness=0, bd=0,
                   relief="flat").pack(side="left")


#==========#
# VWFGroup #
#==========#
class VWFGroup:
    """One row in the calibration table: Voltage, Watts, Frequency."""

    def __init__(
        self,
        parent: tk.Widget,
        index: int,
        accent: str,
        *,
        on_delete: Callable[[VWFGroup], None],
        on_add: Callable[[], None],
        last_frequency: str,
        on_freq_change: Callable[[str], None],
    ) -> None:
        self.frame = tk.Frame(
            parent, bg=Theme.SURFACE,
            highlightthickness=1, highlightbackground=Theme.BORDER,
        )
        self.frame.pack(fill="x", pady=2, padx=2)

        # Delete button (right-aligned so it doesn't shift other widgets)
        tk.Button(
            self.frame, text="X", command=lambda: on_delete(self),
            bg=Theme.SURFACE, fg=Theme.ERROR,
            activebackground=Theme.SURFACE, activeforeground=Theme.ERROR,
            relief="flat", font=Theme.MID_BOLD, cursor="hand2", bd=0,
            highlightthickness=0,
        ).pack(side="right", padx=6)

        self._badge = tk.Label(
            self.frame, text=f"#{index + 1}", width=3,
            bg=accent, fg="white", font=Theme.MID_BOLD,
        )
        self._badge.pack(side="left", padx=(4, 6), pady=4)

        self.v_var, self.v_entry = self._make_field("V:",  6)
        self.w_var, self.w_entry = self._make_field("W:",  6,  left_pad=6)
        self.f_var, self.f_entry = self._make_field("Hz:", 12, left_pad=6, initial=last_frequency)

        for entry in (self.v_entry, self.w_entry, self.f_entry):
            entry.bind("<Return>", lambda _: on_add())
            entry.bind("<KP_Enter>", lambda _: on_add())

        self._trace_ids = [
            self.v_var.trace_add("write", lambda *_: self._on_change(on_freq_change)),
            self.w_var.trace_add("write", lambda *_: self._on_change(on_freq_change)),
            self.f_var.trace_add("write", lambda *_: self._on_change(on_freq_change)),
        ]

    ### private
    def _value_range_validation(self, value: str, lower_bound: float, upper_bound: float) -> bool:
        try:
            parsed_value = float(value)
            return lower_bound <= parsed_value <= upper_bound
        except ValueError:
            return False

    def _make_field(
        self, label: str, width: int,
        left_pad: int = 0, initial: str = ""
    ) -> tuple[tk.StringVar, tk.Entry]:
        tk.Label(self.frame, text=label, bg=Theme.SURFACE,
                 fg=Theme.MUTED, font=Theme.MID).pack(side="left", padx=(left_pad, 0))
        var = tk.StringVar(value=initial)
        entry = make_entry(self.frame, textvariable=var, width=width)
        entry.pack(side="left", padx=3, pady=4)
        return var, entry

    def _error_entry(self, entry: tk.Entry, highlight_while_focused:bool = True) -> None:
        entry.config(highlightbackground=Theme.ERROR)
        
        # show error immeditately (while typing)        
        if highlight_while_focused:
            entry.config(highlightcolor=Theme.ERROR)

    def _on_change(self, on_freq_change: Callable[[str], None]) -> None:
        filled = {
            "v": self.v_var.get().strip() != "",
            "w": self.w_var.get().strip() != "",
            "f": self.f_var.get().strip() != "",
        }
        any_filled = any(filled.values())

        # value check each field: non-empty, parseable as float
        for entry, key in (
            (self.v_entry, "v"),
            (self.w_entry, "w"),
            (self.f_entry, "f"),
        ):
            entry.config(highlightbackground=(
                Theme.WARN if any_filled and not filled[key] else Theme.BORDER
            ), highlightcolor=Theme.ACCENT)

        # voltage validation
        if filled["v"]:
            lower, upper = RangeValidation.voltage_lower_bound, RangeValidation.voltage_upper_bound
            valid = self._value_range_validation(self.v_var.get().strip(), lower, upper)
            if not valid:
                self._error_entry(self.v_entry)

        # watt validation
        if filled["w"]:
            lower, upper = RangeValidation.watt_lower_bound, RangeValidation.watt_upper_bound
            valid = self._value_range_validation(self.w_var.get().strip(), lower, upper)
            if not valid:
                self._error_entry(self.w_entry)

        # frequency validation, store last valid input for pre-filling new rows
        if filled["f"]:
            lower, upper = 0, float('inf') # accept any non-negative frequency
            valid = self._value_range_validation(self.f_var.get().strip(), lower, upper)
            if not valid:
                # frequency can be invalid while typing (e.g. "430e" when typing "430e6"), so only show error when focus leaves the field
                self._error_entry(self.f_entry, highlight_while_focused=False)
            else:
                # store valid frequency
                on_freq_change(self.f_var.get().strip())

    ### public
    def renumber(self, index: int) -> None:
        self._badge.config(text=f"#{index + 1}")

    def get(self) -> tuple[str, str, str]:
        """Return (voltage, watts, frequency) as raw strings."""
        return (
            self.v_var.get().strip(),
            self.w_var.get().strip(),
            self.f_var.get().strip(),
        )

    def destroy(self) -> None:
        for var, tid in zip(
            (self.v_var, self.w_var, self.f_var), self._trace_ids,
        ):
            try:
                var.trace_remove("write", tid)
            except Exception:
                pass
        self.frame.destroy()


#================#
# PredictorPanel #
#================#
class PredictorPanel:
    """(voltage, frequency) -> watt prediction widget."""

    def __init__(
        self,
        parent: tk.Widget,
        hpa_index: int,
        accent: str,
        get_result: Callable[[int], CalibrationResult | None],
    ) -> None:
        self.hpa_index   = hpa_index
        self._get_result = get_result

        outer = make_card(parent)
        outer.pack(fill="x", padx=6, pady=6)

        hdr = tk.Frame(outer, bg=accent)
        hdr.pack(fill="x")
        tk.Label(hdr, text=f"  HPA {hpa_index} -- Prediction Tool",
                 bg=accent, fg="white", font=Theme.LARGE_BOLD, pady=8).pack(side="left")

        inner = tk.Frame(outer, bg=Theme.CARD, pady=10)
        inner.pack(fill="x", padx=10)

        self.v_var = tk.StringVar()
        self.f_var = tk.StringVar()
        self.w_var = tk.StringVar(value="0.00")

        rows = (
            (0, "Input Voltage (V):",    Theme.MUTED,   Theme.MID,      self.v_var, True),
            (1, "Input Frequency (Hz):", Theme.MUTED,   Theme.MID,      self.f_var, True),
            (2, "Predicted Watts (W):",  Theme.SUCCESS, Theme.MID_BOLD, self.w_var, False),
        )
        for row, text, fg, font, var, editable in rows:
            tk.Label(inner, text=text, bg=Theme.CARD, fg=fg, font=font).grid(
                row=row, column=0, sticky="w", pady=2 if row < 2 else 10,
            )
            if editable:
                make_entry(inner, textvariable=var, width=12).grid(
                    row=row, column=1, sticky="w", padx=10, pady=2,
                )
            else:
                tk.Label(
                    inner, textvariable=var,
                    bg=Theme.ENTRY_BG, fg=Theme.SUCCESS, font=Theme.LARGE_BOLD,
                    width=12, relief="flat", highlightthickness=1,
                    highlightbackground=Theme.BORDER,
                ).grid(row=row, column=1, sticky="w", padx=10, pady=10)

        self.v_var.trace_add("write", lambda *_: self.refresh())
        self.f_var.trace_add("write", lambda *_: self.refresh())

    def refresh(self) -> None:
        try:
            v = float(self.v_var.get().strip())
            f = float(self.f_var.get().strip())
        except ValueError:
            self.w_var.set("---")
            return

        result = self._get_result(self.hpa_index)
        if result is None:
            self.w_var.set("No Data")
            return

        self.w_var.set(f"{PowerModel.predict(v, f, result):.2f}")


#==========#
# HPAPanel #
#==========#
class HPAPanel:
    """Scrollable calibration input panel."""

    _GUIDE_TEXT = (
        "Guide: Perform 3+ (5-6 recommended) V/W measurements at 3 frequencies spanning "
        "the full operating range. E.g. for 400-480 MHz, measure at gains 20-30 and at "
        "400e6, 440e6, 480e6 Hz."
    )

    def __init__(
        self,
        parent: tk.Widget,
        hpa_index: int,
        accent: str,
        on_fit_done: Callable[[int, CalibrationResult], None],
    ) -> None:
        self.hpa_index   = hpa_index
        self._accent     = accent
        self._on_fit_done = on_fit_done

        self._last_frequency: str   = ""
        self._rows: list[VWFGroup]  = []

        outer = make_card(parent)
        outer.pack(fill="both", expand=True, padx=6, pady=6)

        self._build_header(outer, accent)
        self._build_scroll_area(outer)
        self._build_buttons(outer, accent)
        self._build_status(outer)

        self.add_row()

    ### construction helpers
    def _build_header(self, parent: tk.Widget, accent: str) -> None:
        hdr = tk.Frame(parent, bg=accent)
        hdr.pack(fill="x")
        tk.Label(hdr, text=f"  HPA {self.hpa_index} -- Calibration",
                 bg=accent, fg="white", font=Theme.LARGE_BOLD, pady=8).pack(side="left")

    def _build_scroll_area(self, parent: tk.Widget) -> None:
        container = tk.Frame(parent, bg=Theme.SURFACE)
        container.pack(fill="both", expand=True, padx=6, pady=6)

        self._canvas = tk.Canvas(container, bg=Theme.SURFACE, highlightthickness=0)
        scrollbar = ttk.Scrollbar(container, orient="vertical", command=self._canvas.yview)
        scrollbar.pack(side="right", fill="y")

        self._rows_frame = tk.Frame(self._canvas, bg=Theme.SURFACE)
        self._rows_frame.bind(
            "<Configure>",
            lambda _: self._canvas.configure(scrollregion=self._canvas.bbox("all")),
        )

        win = self._canvas.create_window((0, 0), window=self._rows_frame, anchor="nw")
        self._canvas.bind("<Configure>", lambda e: self._canvas.itemconfig(win, width=e.width))
        self._canvas.configure(yscrollcommand=scrollbar.set)
        self._canvas.pack(side="left", fill="both", expand=True)
        self._canvas.bind("<MouseWheel>", self._on_scroll)

    def _build_buttons(self, parent: tk.Widget, accent: str) -> None:
        bar = tk.Frame(parent, bg=Theme.CARD)
        bar.pack(fill="x", padx=6, pady=(0, 8))
        make_button(bar, "+ Add Row",         self.add_row, color=Theme.SURFACE, width=14).pack(side="left", padx=4)
        make_button(bar, "> Run Calibration", self._run,   color=accent,         width=14).pack(side="left", padx=4)

    def _build_status(self, parent: tk.Widget) -> None:
        frame = tk.Frame(parent, bg=Theme.CARD)
        frame.pack(fill="x", padx=6, pady=4)
        self._status_lbl = tk.Label(
            frame, text=self._GUIDE_TEXT,
            bg=Theme.CARD, fg=Theme.TEXT, font=Theme.MID, justify="left",
        )
        self._status_lbl.pack(fill="both", expand=True)
        frame.bind(
            "<Configure>",
            lambda e: self._status_lbl.config(wraplength=max(1, e.width - 8)),
        )

    ### scroll
    def _on_scroll(self, event: tk.Event) -> None:
        # Prevent scrolling if all content is fully visible
        if self._canvas.yview() == (0.0, 1.0):
            return

        # Linux uses event.num (4 for up, 5 for down)
        # Windows/macOS use event.delta
        if getattr(event, 'num', 0) == 4:
            self._canvas.yview_scroll(-1, "units")
        elif getattr(event, 'num', 0) == 5:
            self._canvas.yview_scroll(1, "units")
        else:
            # Windows/macOS logic
            delta = event.delta if sys.platform == "darwin" else int(event.delta / 120)
            self._canvas.yview_scroll(int(-1 * delta), "units")

    def _bind_scroll_recursive(self, widget: tk.Widget) -> None:
        # Windows/ macOS
        widget.bind("<MouseWheel>", self._on_scroll)
        # Linux (X11)
        widget.bind("<Button-4>", self._on_scroll)
        widget.bind("<Button-5>", self._on_scroll)
        
        for child in widget.winfo_children():
            self._bind_scroll_recursive(child)

    def _scroll_to_bottom(self, delay: int = 25, attempts: int = 20) -> None:
        _, bottom = self._canvas.yview()
        if bottom < 1.0:
            self._canvas.after(2 * delay, lambda: self._canvas.yview_moveto(1.0))
        elif attempts > 0:
            self._canvas.after(delay, lambda: self._scroll_to_bottom(delay, attempts - 1))

    ### row management
    def add_row(self, use_last_freq: bool = True) -> None:
        row = VWFGroup(
            parent=self._rows_frame,
            index=len(self._rows),
            accent=self._accent,
            on_delete=self._delete_row,
            on_add=self.add_row,
            last_frequency=self._last_frequency if use_last_freq else "",
            on_freq_change=lambda f: setattr(self, "_last_frequency", f),
        )
        self._bind_scroll_recursive(row.frame)
        self._rows.append(row)
        self._canvas.after(10, lambda: self._scroll_to_bottom())
        row.v_entry.focus_set()

    def _delete_row(self, row: VWFGroup) -> None:
        row.destroy()
        self._rows.remove(row)
        for i, r in enumerate(self._rows):
            r.renumber(i)
        if not self._rows:
            self.add_row(use_last_freq=False)

    ### data collection
    def _collect_rows(self) -> tuple[list[float], list[float], list[float], list[int]]:
        """
        Parse all rows.  Fully empty rows are silently skipped.

        Returns (v_list, w_list, f_list, bad_row_numbers).
        """
        v_list, w_list, f_list, bad = [], [], [], []
        for i, row in enumerate(self._rows):
            v_s, w_s, f_s = row.get()
            if not any((v_s, w_s, f_s)):
                continue  # blank row
            try:
                v_list.append(float(v_s))
                w_list.append(float(w_s))
                f_list.append(float(f_s))
            except ValueError:
                bad.append(i + 1)
        return v_list, w_list, f_list, bad

    ### calibration
    def _run(self) -> None:
        v_list, w_list, f_list, bad = self._collect_rows()
        if bad:
            messagebox.showwarning(
                "Incomplete Data",
                f"HPA {self.hpa_index}: Rows {bad} are incomplete, non-numeric, or out of bounds.\n\n" +
                f"Parameter bounds: \n" +
                f"  Voltage:\t{RangeValidation.voltage_lower_bound} - {RangeValidation.voltage_upper_bound}V\n" +
                f"  Watt:\t{RangeValidation.watt_lower_bound} - {RangeValidation.watt_upper_bound}W\n"
            )
            return

        try:
            result = PowerModel.fit(
                np.array(v_list), np.array(w_list), np.array(f_list),
            )
        except (ValueError, RuntimeError) as ex:
            messagebox.showerror(f"HPA {self.hpa_index} -- Calibration Error", str(ex))
            return

        self._status_lbl.config(
            fg=Theme.SUCCESS,
            text=(
                "Calibration successful! Parameters written to configuration.\n"
                f"RMSE: {result.rmse:.2f}     R²: {result.r2:.4f}"
            ),
        )
        self._on_fit_done(self.hpa_index, result)


#===================#
# SystemConfigPanel #
#===================#
class SystemConfigPanel:
    """
    Left-column panel: system metadata, channel config, temperature sensors,
    and a read-only coefficient display that is updated after each calibration.
    """

    # Config keys that appear in both channels and temperature sensors sections.
    _DEVICE_KEYS = ("chan0", "chan1", "temperature1", "temperature2")

    def __init__(self, parent: tk.Widget) -> None:
        self._canvas = tk.Canvas(parent, bg=Theme.BG, highlightthickness=0, width=280)
        scrollbar = ttk.Scrollbar(parent, orient="vertical", command=self._canvas.yview)
        self._canvas.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side="right", fill="y")
        self._canvas.pack(side="left", fill="y")

        inner = tk.Frame(self._canvas, bg=Theme.BG)
        win   = self._canvas.create_window((0, 0), window=inner, anchor="nw")
        
        inner.bind("<Configure>", lambda _: self._canvas.configure(scrollregion=self._canvas.bbox("all")))
        self._canvas.bind("<Configure>", lambda e: self._canvas.itemconfig(win, width=e.width))

        self._init_vars()
        self._build(inner)
        
        self._bind_scroll_recursive(self._canvas)
        self._bind_scroll_recursive(inner)

    ### scroll behavior
    def _on_scroll(self, event: tk.Event) -> None:
        # Prevent scrolling if all content is fully visible
        if self._canvas.yview() == (0.0, 1.0):
            return
            
        if getattr(event, 'num', 0) == 4:
            self._canvas.yview_scroll(-1, "units")
        elif getattr(event, 'num', 0) == 5:
            self._canvas.yview_scroll(1, "units")
        else:
            delta = event.delta if sys.platform == "darwin" else int(event.delta / 120)
            self._canvas.yview_scroll(int(-1 * delta), "units")

    def _bind_scroll_recursive(self, widget: tk.Widget) -> None:
        widget.bind("<MouseWheel>", self._on_scroll)
        widget.bind("<Button-4>", self._on_scroll)
        widget.bind("<Button-5>", self._on_scroll)
        for child in widget.winfo_children():
            self._bind_scroll_recursive(child)

    ### variable initialisation
    def _init_vars(self) -> None:
        self.v_system      = tk.StringVar()
        self.v_description = tk.StringVar()

        # Shared var dict for channels and temperature sensors (same fields).
        self._device_vars: dict[str, dict[str, tk.Variable]] = {
            key: {
                "label":    tk.StringVar(),
                "enabled":  tk.BooleanVar(),
                "partName": tk.StringVar(),
            }
            for key in self._DEVICE_KEYS
        }

        # Power-calculation coefficient display vars (read-only in UI).
        self._pc_vars: dict[int, dict[str, tk.Variable]] = {
            idx: {
                "enabled": tk.BooleanVar(),
                **{k: tk.StringVar() for k in (*COEFF_KEYS, "fmin", "fmax")},
            }
            for idx in (1, 2)
        }

    ### UI construction
    def _build(self, parent: tk.Widget) -> None:
        make_section_header(parent, "System")
        sf = make_card(parent)
        sf.pack(fill="x", padx=6, pady=(0, 10))
        make_field_row(sf, "System Name", self.v_system)
        make_field_row(sf, "Description", self.v_description)

        make_section_header(parent, "Channels")
        for key, accent in (("chan0", Theme.HPA1), ("chan1", Theme.HPA2)):
            self._device_card(parent, key, accent)

        make_section_header(parent, "Temperature Sensors")
        for key, accent in (("temperature1", Theme.HPA1), ("temperature2", Theme.HPA2)):
            self._device_card(parent, key, accent)

        make_section_header(parent, "Coefficients (auto-filled =>)")
        for hpa_idx, cfg_key, accent in (
            (1, "powercalculation1", Theme.HPA1),
            (2, "powercalculation2", Theme.HPA2),
        ):
            pf    = make_card(parent)
            pvars = self._pc_vars[hpa_idx]
            pf.pack(fill="x", padx=6, pady=3)
            tk.Frame(pf, bg=accent, height=4).pack(fill="x")
            make_label(pf, cfg_key, bold=True, color=accent,
                       bg=Theme.CARD, padx=8, pady=4).pack(anchor="w")
            make_check_row(pf, "Enabled", pvars["enabled"])
            for key in (*COEFF_KEYS, "fmin", "fmax"):
                make_field_row(pf, key, pvars[key], readonly=True)

    def _device_card(self, parent: tk.Widget, key: str, accent: str) -> None:
        cf   = make_card(parent)
        dv   = self._device_vars[key]
        cf.pack(fill="x", padx=6, pady=3)
        tk.Frame(cf, bg=accent, height=4).pack(fill="x")
        make_label(cf, key, bold=True, color=accent,
                   bg=Theme.CARD, padx=8, pady=4).pack(anchor="w")
        make_field_row(cf, "Label",     dv["label"])
        make_check_row(cf, "Enabled",   dv["enabled"])
        make_field_row(cf, "Part Name", dv["partName"])

    ### public
    def load(self, cfg: AppConfig) -> None:
        """Populate all UI variables from an AppConfig."""
        self.v_system.set(cfg.system)
        self.v_description.set(cfg.description)

        for key, src in (
            ("chan0",        cfg.chan0),
            ("chan1",        cfg.chan1),
            ("temperature1", cfg.temperature1),
            ("temperature2", cfg.temperature2),
        ):
            dv = self._device_vars[key]
            dv["label"].set(src.label)
            dv["enabled"].set(src.enabled)
            dv["partName"].set(src.part_name)

        for hpa_idx, src in ((1, cfg.powercalculation1), (2, cfg.powercalculation2)):
            pv = self._pc_vars[hpa_idx]
            pv["enabled"].set(src.enabled)
            for key in (*COEFF_KEYS, "fmin", "fmax"):
                pv[key].set(str(getattr(src, key)))

    def collect(self) -> AppConfig:
        """Read all UI variables into a fresh AppConfig."""

        def _float(var: tk.StringVar, name: str) -> float:
            try:
                return float(var.get())
            except ValueError:
                messagebox.showwarning(
                    "Invalid Value",
                    f"Field '{name}' is not numeric; defaulted to 0.0",
                )
                return 0.0

        def _channel(key: str) -> ChannelConfig:
            dv = self._device_vars[key]
            return ChannelConfig(
                label=dv["label"].get(),
                enabled=dv["enabled"].get(),
                part_name=dv["partName"].get(),
            )

        def _temp(key: str) -> TempSensorConfig:
            dv = self._device_vars[key]
            return TempSensorConfig(
                label=dv["label"].get(),
                enabled=dv["enabled"].get(),
                part_name=dv["partName"].get(),
            )

        def _pc(hpa_idx: int) -> PowerCalcConfig:
            pv = self._pc_vars[hpa_idx]
            return PowerCalcConfig(
                enabled=pv["enabled"].get(),
                **{k: _float(pv[k], k) for k in (*COEFF_KEYS, "fmin", "fmax")},
            )

        return AppConfig(
            system=self.v_system.get(),
            description=self.v_description.get(),
            chan0=_channel("chan0"),
            chan1=_channel("chan1"),
            temperature1=_temp("temperature1"),
            temperature2=_temp("temperature2"),
            powercalculation1=_pc(1),
            powercalculation2=_pc(2),
        )

    def update_coefficients(self, hpa_idx: int, result: CalibrationResult) -> None:
        """Refresh the read-only coefficient display after a successful fit."""
        pv = self._pc_vars[hpa_idx]
        for key in COEFF_KEYS:
            pv[key].set(str(getattr(result, key)))
        pv["fmin"].set(f"{result.fmin:.0f}")
        pv["fmax"].set(f"{result.fmax:.0f}")
        pv["enabled"].set(True)


#=============#
# Application #
#=============#
class SequencerCalibrationApp(tk.Tk):
    _WINDOW_W = 1280
    _WINDOW_H = 820

    def __init__(self) -> None:
        super().__init__()
        self.title("Sequencer Calibration Tool")
        self.configure(bg=Theme.BG)
        self.minsize(1280, 700)
        self._center_window()
        self.protocol("WM_DELETE_WINDOW", self._close)

        # Calibration results keyed by HPA index (1 or 2).
        self._results: dict[int, CalibrationResult] = {}

        self._build_toolbar()
        self._build_body()

        # Seed UI with default config values.
        self._config_panel.load(AppConfig())

    ### window setup
    def _center_window(self) -> None:
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        self.geometry(
            f"{self._WINDOW_W}x{self._WINDOW_H}"
            f"+{(sw - self._WINDOW_W) // 2}+{(sh - self._WINDOW_H) // 2}"
        )

    def _close(self) -> None:
        self.quit()
        self.destroy()

    ### UI construction
    def _build_toolbar(self) -> None:
        bar = tk.Frame(self, bg=Theme.SURFACE, pady=8)
        bar.pack(fill="x", side="top")
        make_button(bar, "Save config.json", self._save_config,
                    color=Theme.SUCCESS, width=20).pack(side="right", padx=8)

    def _build_body(self) -> None:
        body = tk.Frame(self, bg=Theme.BG)
        body.pack(fill="both", expand=True, padx=10, pady=6)

        body.columnconfigure(0, weight=0, minsize=280)
        body.columnconfigure(1, weight=3, minsize=380)
        body.columnconfigure(2, weight=3, minsize=380)
        body.rowconfigure(0, weight=1)

        col_left  = tk.Frame(body, bg=Theme.BG)
        col_mid   = tk.Frame(body, bg=Theme.BG)
        col_right = tk.Frame(body, bg=Theme.BG)

        col_left.grid(row=0,  column=0, sticky="nsew", padx=(0, 4))
        col_mid.grid(row=0,   column=1, sticky="nsew", padx=4)
        col_right.grid(row=0, column=2, sticky="nsew", padx=(4, 0))

        self._config_panel = SystemConfigPanel(col_left)
        self._build_hpa_column(col_mid,   hpa_index=1, accent=Theme.HPA1)
        self._build_hpa_column(col_right, hpa_index=2, accent=Theme.HPA2)

    def _build_hpa_column(
        self, parent: tk.Widget, hpa_index: int, accent: str,
    ) -> None:
        HPAPanel(parent, hpa_index, accent, self._on_fit_done)
        PredictorPanel(parent, hpa_index, accent, self._get_result)

    ### calibration callbacks
    def _on_fit_done(self, hpa_index: int, result: CalibrationResult) -> None:
        self._results[hpa_index] = result
        self._config_panel.update_coefficients(hpa_index, result)

    def _get_result(self, hpa_index: int) -> CalibrationResult | None:
        return self._results.get(hpa_index)

    ### config I/O
    def _save_config(self) -> None:
        path = filedialog.asksaveasfilename(
            defaultextension=".json",
            filetypes=[("JSON files", "*.json")],
            initialfile="config.json",
            title="Save config.json",
        )
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(self._config_panel.collect().to_dict(), fh, indent=2)
            messagebox.showinfo("Saved", f"config.json saved to:\n{path}")
        except OSError as ex:
            messagebox.showerror("Save Failed", f"Could not write file:\n{ex}")

    def _load_config(self) -> None:
        path = filedialog.askopenfilename(
            filetypes=[("JSON files", "*.json")],
            title="Load config.json",
        )
        if not path:
            return
        try:
            with open(path, encoding="utf-8") as fh:
                cfg = AppConfig.from_dict(json.load(fh))
            self._config_panel.load(cfg)
            messagebox.showinfo("Loaded", f"Config loaded from:\n{path}")
        except (OSError, json.JSONDecodeError) as ex:
            messagebox.showerror("Load Failed", f"Could not read config:\n{ex}")

#=============#
# Entry point #
#=============#
if __name__ == "__main__":
    SequencerCalibrationApp().mainloop()