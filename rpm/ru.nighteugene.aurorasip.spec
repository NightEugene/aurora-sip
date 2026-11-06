Name: ru.nighteugene.aurorasip
Summary: Aurora SIP — native IP phone
Version: 1.0.0
Release: 5
License: GPL-2.0-or-later
Source0: %{name}-%{version}.tar.xz

BuildRequires: patch
BuildRequires: tar
BuildRequires: pkgconfig(qca2-qt5)
BuildRequires: pkgconfig(ngf-qt5)
BuildRequires: pkgconfig(callservice)
BuildRequires: pkgconfig(auroraapp)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Multimedia)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(openssl)

Requires: sailfishsilica-qt5
Requires: aurora-controls

%description
TLS SIP audio phone for FreePBX.

%prep
%autosetup

%build
# Apply the TLS identity patch only to a disposable build copy.
pj_source=%{_builddir}/pjproject
rm -rf "$pj_source"
mkdir -p "$pj_source"
tar -C /workspace/third_party/pjproject --exclude=.git -cf - . | tar -C "$pj_source" -xf -
cd "$pj_source"
patch -p1 < /workspace/patches/pjsip-tls-hostname.patch
cp /workspace/patches/aurora_tls_identity.h pjsip/src/pjsip/aurora_tls_identity.h

target_triple=${PKG_CONFIG_SYSROOT_DIR%/sys-root}
target_triple=${target_triple##*/}
CFLAGS="%{optflags} -fPIC" ./configure --host="$target_triple" --prefix=%{_builddir}/pj-install --with-ssl="$PKG_CONFIG_SYSROOT_DIR/usr" --disable-opus --disable-libuuid --disable-sound --disable-video --disable-opencore-amr --disable-libyuv --disable-libwebrtc --disable-ffmpeg --disable-v4l2 --disable-sdl
make -j4
make install

export PKG_CONFIG_PATH=%{_builddir}/pj-install/lib/pkgconfig:$PKG_CONFIG_PATH
export PBX_PREFIX=%{_builddir}/pj-install
cd %{_builddir}
%qmake5 /workspace/aurora-sip.pro
%make_build

%install
%make_install

install -m 644 /workspace/LICENSE %{buildroot}%{_datadir}/%{name}/LICENSE
install -m 644 /workspace/third_party/pjproject/COPYING %{buildroot}%{_datadir}/%{name}/PJSIP-COPYING
notice_dir=%{buildroot}%{_datadir}/%{name}/licenses
mkdir -p "$notice_dir"
cd /workspace/third_party/pjproject
find third_party -type f \( -iname 'COPYING*' -o -iname 'COPYRIGHT*' -o -iname 'LICENSE*' \) |
while IFS= read -r notice; do
    install -D -m 644 "$notice" "$notice_dir/$notice"
done
install -D -m 644 third_party/ilbc/iLBC_decode.c "$notice_dir/third_party/ilbc/iLBC_decode.c"
install -D -m 644 third_party/g7221/encode/encoder.c "$notice_dir/third_party/g7221/encode/encoder.c"
install -m 644 third_party/README.txt "$notice_dir/THIRD-PARTY-README.txt"

%files
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
