# Compilar, empacotar e publicar

[← Voltar ao README](../README.md)

## Requisitos

| | Linux | Windows | macOS |
|---|---|---|---|
| Compilador | GCC ou Clang com C++17 | Visual Studio 2022 (workload "Desenvolvimento para desktop com C++") | Xcode Command Line Tools (`xcode-select --install`) |
| CMake | ≥ 3.16 | ≥ 3.16 (já vem no Visual Studio) | ≥ 3.16 (`brew install cmake`) |
| Outros | `git` e as bibliotecas abaixo | `git` | `git` |

A [raylib 5.5](https://www.raylib.com/) é baixada e compilada automaticamente na primeira compilação (CMake FetchContent).

Bibliotecas de desenvolvimento no Linux:

```bash
# Ubuntu / Debian
sudo apt-get install -y git cmake build-essential pkg-config \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxi-dev \
  libxinerama-dev libxcursor-dev libasound2-dev

# Fedora
sudo dnf install -y git cmake gcc-c++ libstdc++-static mesa-libGL-devel \
  libX11-devel libXrandr-devel libXi-devel libXinerama-devel \
  libXcursor-devel alsa-lib-devel
```

## Compilar e rodar

```bash
git clone git@github.com:chmenegatti/warp-menu.git
cd warp-menu
cmake -S . -B build
cmake --build build --parallel
```

| Sistema | Como rodar o resultado |
|---|---|
| Linux | `./build/warp-menu`, ou `./warp.sh`, que recompila sozinho quando o código muda |
| Windows | `build\Release\warp-menu.exe` (a pasta `assets` é copiada ao lado) |
| macOS | `open build/warp-menu.app` (os assets vão dentro do bundle) |

No Linux, para ter o comando `warp` apontando para a cópia de desenvolvimento:

```bash
ln -sf "$PWD/warp.sh" ~/.local/bin/warp
```

### Como o app acha os assets

Em tempo de execução, nesta ordem:

1. `<pasta do executável>/assets`: Windows e cópias soltas.
2. `<pasta do executável>/../share/warp-menu/assets`: pacotes Linux (`/usr/bin` → `/usr/share/warp-menu`).
3. `<pasta do executável>/../Resources/assets`: bundle `.app` do macOS.
4. A pasta `assets/` do código-fonte, gravada na compilação: builds de desenvolvimento.

## Adicionar ou alterar vnets

As vnets ficam em `src/warp.cpp`, no construtor do `WarpController`:

```cpp
vnets_ = {
    {"default", "51d02f9b-41ec-4b0a-8691-072aa331acff"},
    {"TESP2",   "a61c9382-38d9-488d-b735-a927532bb006"},
    {"TECE1",   "55f51d19-3faf-43d9-9ddc-646a557edd1f"},
};
```

Adicione uma linha `{"NOME", "uuid"}`. Cada vnet vira uma taverna com o nome em maiúsculas, e a rua fica mais longa. Os UUIDs das redes virtuais da organização aparecem em `warp-cli vnet`. Para distribuir a mudança, publique uma nova versão (abaixo).

## Gerar os pacotes

### Linux (.deb + .rpm)

Os pacotes são gerados dentro de um container **Rocky Linux 9**. A glibc antiga garante que o binário rode no Ubuntu 22.04+, Debian 12+, Fedora e RHEL/Rocky 9:

```bash
docker run --rm --network host -v "$PWD":/src -w /src -e HOST_UID="$(id -u)" \
    rockylinux:9 sh packaging/linux/build-packages.sh
ls build-pkg/*.deb build-pkg/*.rpm
```

(`--network host` evita problemas de DNS dentro do container quando o próprio WARP está conectado.)

O `libstdc++` e o `libgcc` são linkados estaticamente. As bibliotecas de X11, OpenGL e ALSA são carregadas em tempo de execução, por isso estão declaradas à mão como dependências no `CMakeLists.txt`.

### Windows e macOS

São gerados pelo GitHub Actions (`.github/workflows/build.yml`):

- **Windows:** compila com MSVC e gera um `.zip` com `warp-menu.exe` e a pasta `assets`.
- **macOS:** compila um binário universal (`arm64` + `x86_64`, macOS 11+), assina o `.app` ad-hoc (`codesign -s -`, exigido no Apple Silicon) e gera o `.dmg`. O app **não é notarizado**: para isso seria preciso uma conta Apple Developer paga.

O workflow roda a cada push na `main` e em pull requests. Os pacotes ficam disponíveis como *artifacts* na página da execução, em **Actions**.

## Publicar uma versão

1. Atualize a versão no `CMakeLists.txt`:

   ```cmake
   project(warp_menu VERSION 1.1.0 ...)
   ```

2. Faça commit e crie a tag com a **mesma** versão:

   ```bash
   git commit -am "release: 1.1.0"
   git tag v1.1.0
   git push origin main v1.1.0
   ```

3. O workflow compila os três sistemas e cria a Release `v1.1.0` com os quatro arquivos anexados. Acompanhe em **Actions** ou com `gh run watch`.

Os nomes dos arquivos vêm da versão do `CMakeLists.txt`, não da tag. Por isso as duas precisam ser iguais.

## Verificação sem interface

O app tem um modo de print que não executa nenhuma ação do WARP, útil para conferir mudanças visuais e testar pacotes:

```bash
WARP_SNAPSHOT=/tmp/tela.png ./build/warp-menu
WARP_SNAPSHOT=/tmp/porta.png WARP_SNAPSHOT_WIZ=392,141 ./build/warp-menu   # mago na 3ª porta
```

Em container ou servidor sem tela, use o Xvfb:

```bash
xvfb-run -a -s '-screen 0 1024x768x24' env WARP_SNAPSHOT=/tmp/tela.png warp-menu
```
