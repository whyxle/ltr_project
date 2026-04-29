# LTR Qt Capture Tool

Qt5 application for working with LTR114/LTR212 modules.

## External dependencies

The `LTR/` directory is intentionally not tracked. It contains LTRAPI headers,
DLLs and import libraries from the module vendor. To build the project locally,
copy or install the required LTRAPI files so that the paths used by
`ltr_test.pro` are available:

- `LTR/ltrapi.h`
- `LTR/ltr11api.h`
- `LTR/ltr114api.h`
- `LTR/ltr212api.h`
- `LTR/libltrapi.a`
- `LTR/libltr11api.a`
- `LTR/libltr114api.a`
- `LTR/libltr212api.a`

