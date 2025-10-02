Package: openssl:x64-linux@3.5.3

**Host Environment**

- Host: x64-linux
- Compiler: GNU 15.2.1
- CMake Version: 3.31.6
-    vcpkg-tool version: 2025-09-03-4580816534ed8fd9634ac83d46471440edd82dfe
    vcpkg-scripts version: 0d9d468435 2025-09-30 (4 hours ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
-- Using cached openssl-openssl-openssl-3.5.3.tar.gz
-- Cleaning sources at /var/home/leland/vcpkg/buildtrees/openssl/src/nssl-3.5.3-78bf900265.clean. Use --editable to skip cleaning for the packages you specify.
-- Extracting source /var/home/leland/vcpkg/downloads/openssl-openssl-openssl-3.5.3.tar.gz
-- Applying patch cmake-config.patch
-- Applying patch command-line-length.patch
-- Applying patch script-prefix.patch
-- Applying patch windows/install-layout.patch
-- Applying patch windows/install-pdbs.patch
-- Applying patch unix/android-cc.patch
-- Applying patch unix/move-openssldir.patch
-- Applying patch unix/no-empty-dirs.patch
-- Applying patch unix/no-static-libs-for-shared.patch
-- Using source at /var/home/leland/vcpkg/buildtrees/openssl/src/nssl-3.5.3-78bf900265.clean
-- Getting CMake variables for x64-linux
-- Loading CMake variables from /var/home/leland/vcpkg/buildtrees/openssl/cmake-get-vars_C_CXX-x64-linux.cmake.log
openssl requires Linux kernel headers from the system package manager.
   They can be installed on Alpine systems via `apk add linux-headers`.
   They can be installed on Ubuntu systems via `apt install linux-libc-dev`.

-- Getting CMake variables for x64-linux-dbg
-- Getting CMake variables for x64-linux-rel
-- Configuring x64-linux-dbg
CMake Error at scripts/cmake/vcpkg_execute_required_process.cmake:127 (message):
    Command failed: /usr/bin/bash -c "V=1 ./../src/nssl-3.5.3-78bf900265.clean/vcpkg/configure  \"/usr/bin/perl\" \"/var/home/leland/vcpkg/buildtrees/openssl/src/nssl-3.5.3-78bf900265.clean/Configure\" \"linux-x86_64\" \"enable-static-engine\" \"enable-capieng\" \"no-tests\" \"no-docs\" \"enable-ec_nistp_64_gcc_128\" \"no-shared\" \"no-module\" \"no-apps\" \"--openssldir=/etc/ssl\" \"--libdir=lib\" \"--disable-silent-rules\" \"--verbose\" \"--disable-shared\" \"--enable-static\" \"--debug\" \"--prefix=/home/leland/Desktop/Code/SolScout_v1.1/ingestor/build/vcpkg_installed/x64-linux/debug\""
    Working Directory: /var/home/leland/vcpkg/buildtrees/openssl/x64-linux-dbg
    Error code: 1
    See logs for more information:
      /var/home/leland/vcpkg/buildtrees/openssl/config-x64-linux-dbg-out.log
      /var/home/leland/vcpkg/buildtrees/openssl/config-x64-linux-dbg-err.log

Call Stack (most recent call first):
  scripts/cmake/vcpkg_configure_make.cmake:866 (vcpkg_execute_required_process)
  ports/openssl/unix/portfile.cmake:111 (vcpkg_configure_make)
  ports/openssl/portfile.cmake:79 (include)
  scripts/ports.cmake:206 (include)



```

<details><summary>/var/home/leland/vcpkg/buildtrees/openssl/config-x64-linux-dbg-out.log</summary>

```
Configuring OpenSSL version 3.5.3 for target linux-x86_64
Using os-specific seed configuration
Created configdata.pm
Running configdata.pm
```
</details>

<details><summary>/var/home/leland/vcpkg/buildtrees/openssl/config-x64-linux-dbg-err.log</summary>

```
+ /usr/bin/perl /var/home/leland/vcpkg/buildtrees/openssl/src/nssl-3.5.3-78bf900265.clean/Configure linux-x86_64 enable-static-engine enable-capieng no-tests no-docs enable-ec_nistp_64_gcc_128 no-shared no-module no-apps --openssldir=/etc/ssl --libdir=lib --debug --prefix=/home/leland/Desktop/Code/SolScout_v1.1/ingestor/build/vcpkg_installed/x64-linux/debug
Can't locate File/Copy.pm in @INC (you may need to install the File::Copy module) (@INC entries checked: /usr/local/lib64/perl5/5.40 /usr/local/share/perl5/5.40 /usr/lib64/perl5/vendor_perl /usr/share/perl5/vendor_perl /usr/lib64/perl5 /usr/share/perl5) at configdata.pm line 22980.
BEGIN failed--compilation aborted at configdata.pm line 22980.
```
</details>

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "ingestor",
  "version-string": "1.0.0",
  "dependencies": [
    "nlohmann-json",
    "spdlog",
    "libpqxx",
    "redis-plus-plus",
    "cpr",
    "gtest"
  ]
}

```
</details>
