# SIMKL issue #24 test build

Unconfirmed fix for SIMKL phone/QR device sign-in. Freshly compiled from [65dff3e871b7bd2a78dc827d76bfff0a76efedcb](https://github.com/HandshakeHackin/Nuvio-maxd-releases/tree/65dff3e871b7bd2a78dc827d76bfff0a76efedcb), not the old 1.0.0.28 ZIP.

Download `NuvioMaxd-SIMKL-issue24-65dff3e-test.zip` and install its `PPSA99288` folder using your usual ZIP installation method with the app closed.

After selecting your Nuvio profile, open Settings → Account & Services → Tracking accounts → SIMKL → Connect with phone / QR code. Scan the code or open https://simkl.com/pin and approve with your regular SIMKL account. No developer account or client secret is needed. Connecting does not change Library Source or Watch Progress; choose SIMKL separately if you want to test those.

This unsigned test build has a dismissible “Unverified build” notice. Physical PS5 testing is still needed. Please report whether sign-in connects in [issue #24](https://github.com/HandshakeHackin/Nuvio-maxd-releases/issues/24).

`SHA256SUMS` verifies the ZIP. `BUILD_VERIFICATION.json` records build-time checks, including native executable/configuration validation and the three unused SDK command-line tool build-ID exceptions. Its publication flags describe the state when the build was verified, before this test package was shared.

This test is not a GitHub app release or store update.
