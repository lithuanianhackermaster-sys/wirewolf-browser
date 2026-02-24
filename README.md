# Custom Gecko-based Browser (Windows)

This workspace contains a simple skeleton for a Windows-only browser written in C++ using Qt for the GUI and Microsoft Edge WebView2 as the rendering engine.  The project is named **Wirewolf** and builds an executable called `Wirewolf.exe`.  

**⚠️ Disclaimer:**

- Embedding Gecko is non-trivial. Gecko is the heart of Firefox and is maintained by Mozilla.  
- Building Gecko from source requires the Mozilla build system (`mach`, `clang`, etc.).  
- Prebuilt SDKs (XULRunner, unbranded `firefox.exe` / `libxul.dll`) can be used, but obtaining them is outside the scope of this repo.

## Prerequisites

1. **Windows 10/11**
2. **Qt 6 (or Qt 5)** installed (MSVC version) – the examples assume Qt 6.
3. **CMake** (3.20+).
4. **Microsoft Edge WebView2 runtime** (Evergreen or fixed‑version).  Install via the installer or use `winget install --id Microsoft.WebView2Runtime -e`.
5. **WebView2 SDK** (headers and loader library) downloaded from Microsoft; point `WEBVIEW2_SDK_PATH` at the unpacked folder when configuring CMake.

(If you previously experimented with Gecko you can ignore the following.)

> **Historical notes:**
> - Earlier versions of this project used the Gecko engine; code and instructions still exist in the repo but are now superseded by WebView2.
> - Gecko SDKs remain large and difficult to obtain; switching to WebView2 dramatically simplifies build and deployment.

## Project layout

```
My own browser/
├── CMakeLists.txt
├── README.md
└── src/
    └── main.cpp
```

## Build instructions

### Using CMake (recommended)

```powershell
cd "c:\Users\rapol\Music\ManoZaidimai\My own browser"
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\msvc2019_64" \
      -DWEBVIEW2_SDK_PATH="C:\path\to\webview2\SDK" ..
cmake --build . --config Release
```

Paths above must point to your Qt installation and the WebView2 SDK directory. The resulting executable is `Wirewolf.exe`.

### Alternative: using Qt's qmake (no CMake required)

If you cannot install CMake, you can build with `qmake` (installed with Qt).
Open a **"Qt for Desktop"** command prompt from the Start menu (it sets up environment variables) and run:

```powershell
cd "c:\Users\rapol\Music\ManoZaidimai\My own browser"
qmake -r "CONFIG += release" \
      "WEBVIEW2_SDK_PATH=\"C:\path\to\webview2\SDK\""
nmake      # or mingw32-make if you use MinGW
```

This will generate a Visual Studio project and compile `Wirewolf.exe` as well.  The `-r` flag recurses into `src`; you can also run `qmake` inside `src` if you prefer.

With either method the build output ends up in the same directory structure and you continue packaging in the same way.

The resulting `Wirewolf.exe` will show a Qt window with a WebView2-powered view.  Place a `logo.png` file into `src/icons/` to change the app icon; the resource file already references this image.

### Pushing the project to GitHub

If you want someone else (or GitHub Actions) to build the executable for you, push this workspace to a repository.  Replace `lithuanianhackermaster-sys` with your GitHub username if different.

```powershell
# first tell Git who you are (this only needs to be done once)
git config --global user.name "Your Name"
git config --global user.email "you@example.com"

cd "c:\Users\rapol\Music\ManoZaidimai\My own browser"
git init
git add .
git commit -m "Initial commit"
git branch -M main
git remote add origin https://github.com/lithuanianhackermaster-sys/wirewolf-browser.git
git push -u origin main
```

The name and email above are just placeholders; substitute your real name and an actual email address so Git can label your commits correctly.

Once the repository is created, you can follow the GitHub Actions instructions below to obtain a ready-made `Wirewolf.exe`.

### Creating a standalone package

Because Wirewolf depends on the WebView2 runtime you can make a fully self-contained distribution by bundling the fixed-version runtime alongside the executable and, if desired, creating an installer:  

1. **Download the x64 fixed-version WebView2 runtime** (`.cab`) from Microsoft.  
2. **Extract** it with `expand` (or any archive tool) into a temporary folder:  
   ```powershell
   mkdir C:\webview2-runtime
   expand -F:* path\to\Microsoft.WebView2.FixedVersionRuntime.*.x64.cab C:\webview2-runtime
   ```
3. **Copy the needed files** into your application output directory (`build\Release`):
   - `msedgewebview2.exe`, `msedge.dll`, `WebView2Loader.dll` (the loader is already copied by CMake),
   - any other DLLs from the runtime folder such as `d3dcompiler_47.dll`, `icudtl.dat`, etc.  You can simply copy the entire extracted runtime folder next to `Wirewolf.exe` or place it in a subfolder named `runtimes`.
4. **Package with Qt dependencies** – if you linked Qt dynamically you must also deploy the Qt DLLs (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Sql.dll`, plus plugins for platforms, etc.).  Use `windeployqt` to automate:
   ```powershell
   & "C:\Qt\6.6.0\msvc2019_64\bin\windeployqt.exe" "build\Release\Wirewolf.exe"
   ```
   This will copy all the required Qt libraries and plugins into the release folder.
5. **Test the folder** by running `Wirewolf.exe` on a clean Windows system (e.g. VM) that does not have the WebView2 runtime installed – it should run because the fixed-runtime DLLs are present.

#### No local tools? Use continuous integration (GitHub Actions)

If you cannot install anything on your PC (no CMake, no Qt, etc.), the easiest workaround is to build the program online and download the resulting artifact.  Add the following GitHub Actions workflow to a repository containing this project (`.github/workflows/build.yml`):

```yaml
name: Build Wirewolf
on: [push]

jobs:
  build:
    runs-on: windows-latest
    steps:
    - uses: actions/checkout@v3
    - name: Install Qt
      run: |
        choco install qt --version=6.6.0 -y
    - name: Install CMake
      run: choco install cmake -y
    - name: Configure
      run: |
        mkdir build && cd build
        cmake -DCMAKE_PREFIX_PATH="C:\Qt\6.6.0\msvc2019_64" -DWEBVIEW2_SDK_PATH="C:\webview2-sdk" ..
    - name: Build
      run: cmake --build build --config Release
    - name: Archive
      uses: actions/upload-artifact@v3
      with:
        name: wirewolf-exe
        path: build\Release\Wirewolf.exe
```

You’ll also need to check your WebView2 SDK into the repo or download it during the workflow (e.g. via `Invoke-WebRequest`).  After you push, head to the “Actions” tab on GitHub, open the build run, and download the `wirewolf-exe` artifact – that’s a ready‑to‑run executable you can copy to your machine.

Using CI means you never have to install anything locally: GitHub’s servers do the compilation, and you simply fetch the binary.

#### Option: portable Qt on USB

You can also avoid installations by using a portable Qt/MinGW bundle on a flash drive.  Download the Qt online installer, install it to the USB device (choose the non-admin installation path), then run `qmake`/`mingw32-make` from that drive.  Your dad won’t have to allow anything on the main PC.

#### Packaging installers

To produce an MSI or single EXE installer (NSIS/Inno), follow the earlier steps; these tools do not require pins or admin rights to run if you create the package elsewhere and simply give the resulting installer to another user.

In summary:
- easiest when you cannot install anything: **use GitHub Actions or another CI service** to build the exe for you
- next easiest: **portable Qt** on USB and qmake
- final: install CMake/Qt normally and build locally

Once you have the built executable, you can distribute it as a “standalone” program exactly as described previously.


## Next steps / ideas

- Create a toolbar and address bar styled like Lux Browser (dark theme, rounded corners, custom icon).  The sample includes
  navigation buttons (back/forward/reload/home), a dark stylesheet and Unicode icons for a polished look.  Tabs are closable and the address bar updates with the current URL.
- Implement security/privacy features: toggles for JavaScript and cookies, ability to open new tabs in private mode (no history saved), and a clear‑history command.  These options appear under **Security** in the menu bar.
- Add code to set DuckDuckGo as default search engine (see `main.cpp` comments).  
- Implement import routines for Chrome/Firefox profiles (parsing their SQLite/JSON files).  The current build provides **File → Import Chrome Logins**, which opens a profile folder and scans the `Login Data` SQLite database; decrypted credentials are printed to the debug console using Windows DPAPI.  A similar mechanism could be added for Firefox (`logins.json` and `key4.db`).
- Add password manager: store encrypted credentials, integrate with OS keychain/DPAPI.  
- Package using NSIS/Inno Setup and add application icon for taskbar.

This README will grow as the project develops.