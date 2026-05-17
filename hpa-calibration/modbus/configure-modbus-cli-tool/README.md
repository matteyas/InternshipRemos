# Configure Modbus Device

If the Modbus device reports unexpected values for some reason, perhaps it is configured incorrectly. Running this tool will read the configuration, which should report back 0,0,0,0,0,0,0,0 if it's configured correctly. 0 represents voltage mode, 0-10V. If the values are incorrect, input y and press enter to write the correct configuration.

Use `./build.sh` to build the tool, then run it with `./mode`.