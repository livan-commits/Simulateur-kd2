# Kode Dot CST820 Virtual Part

This is a browser-loadable ESP-EMU Virtual Part for the CST820 touch controller.

## Source of truth

- Pinned SDK: \`kodediy/kode-sdk\` revision \`1841cc8c13d4b2e3e4d7288b40cdac5de207a133\`.
- Driver: \`components/kdrv/cst820/src/kdrv_cst820.c\`.
- Pin/address definitions: \`components/kbsp/board/include/kbsp_pins.h\`.
- The driver expects I2C address \`0x15\`, SDA GPIO4, SCL GPIO5, and a six-byte burst from register \`0x01\`.
- Identity is read as three bytes from \`0xA7\`. The SDK header documents \`0xB7/0x28/0xC3\` as a bench-observed DVT identity, not as a universal datasheet default.
- Shared display/touch reset is active low on GPIO7.

## Implemented behavior

- I2C register-pointer reads and sequential writes.
- DVT identity response and the frame registers \`0x01..0x06\`.
- 12-bit X/Y coordinates and the event/finger fields consumed by the pinned driver.
- Configuration register read/write path \`0xEC..0xFE\`.
- Deep sleep at \`0xE5=0x03\`, NACK while asleep, and wake/reset through GPIO7.
- Browser interactions: \`touch_down:X,Y\`, \`touch_move:X,Y\`, \`touch_up\`, \`swipe_up\`, \`swipe_down\`, \`swipe_left\`, \`swipe_right\`, \`double_tap\`, \`long_press\`.

## Deliberate limits

This is a deterministic digital model of the transactions the pinned KodeOS driver uses. It does not model the capacitive sensing front end, analog effects, scan timing, or the controller's physical interrupt pulse. On the Kode Dot, touch IRQ is routed through TCA6408A P6; a browser page must coordinate that expander input with touch interactions. A passing host test and a successful WASM build do not by themselves prove that a KodeOS firmware image boots or that the browser runtime is fully wired.
