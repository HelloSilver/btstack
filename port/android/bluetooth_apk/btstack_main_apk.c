/*
 * Copyright (C) 2026 BlueKitchen GmbH
 *
 * Bluetooth.apk shim provides its own enable path; no example btstack_main.
 */

int btstack_main(int argc, const char * argv[]){
    (void) argc;
    (void) argv;
    return 0;
}
