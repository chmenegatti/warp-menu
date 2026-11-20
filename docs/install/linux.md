# Instalação no Linux

[← Voltar ao README](../../README.md)

Há dois pacotes, `.deb` e `.rpm`, ambos para **x86_64 (64 bits)**:

| Distribuição | Pacote | Versões testadas |
|---|---|---|
| Ubuntu, Debian, Linux Mint, Pop!_OS | `.deb` | Ubuntu 22.04 e 24.04, Debian 12 |
| Fedora, RHEL, Rocky Linux, AlmaLinux | `.rpm` | Fedora (atual), Rocky Linux 9 |

O app precisa de uma sessão gráfica (X11, ou Wayland via XWayland, que é o padrão no GNOME e no KDE).

## 1. Pré-requisito: Cloudflare WARP

O cliente WARP precisa estar instalado e registrado na organização da TOTVS. Confira num terminal:

```bash
warp-cli status
```

Se o comando não existir, instale o cliente pelo [guia da Cloudflare para Linux](https://developers.cloudflare.com/warp-client/get-started/linux/) e faça o registro com as instruções internas da TOTVS.

## 2. Baixe o pacote

Abra a página de [Releases](https://github.com/chmenegatti/warp-menu/releases/latest) e baixe o arquivo da sua distribuição. Os comandos abaixo supõem que ele está em `~/Downloads`.

## Ubuntu / Debian (.deb)

```bash
cd ~/Downloads
sudo apt install ./warp-menu_*_amd64.deb
```

Use `apt install ./arquivo.deb` em vez de `dpkg -i`, porque o `apt` também instala as bibliotecas gráficas e de áudio de que o app precisa.

## Fedora / RHEL / Rocky (.rpm)

```bash
cd ~/Downloads
sudo dnf install ./warp-menu-*.x86_64.rpm
```

## 3. Abra o app

- **Pelo menu de aplicativos:** procure por **TOTVS WARP**.
- **Pelo terminal:** `warp-menu`

Dica: para ter um comando curto, crie um alias no `~/.bashrc` ou `~/.zshrc`:

```bash
alias warp='warp-menu'
```

## O que é instalado

| Caminho | Conteúdo |
|---|---|
| `/usr/bin/warp-menu` | o executável |
| `/usr/share/warp-menu/assets/` | sprites, fonte e música opcional |
| `/usr/share/applications/warp-menu.desktop` | entrada no menu de aplicativos |
| `/usr/share/icons/hicolor/256x256/apps/warp-menu.png` | ícone |

## Atualizar

Baixe o pacote novo e rode o mesmo comando de instalação. O `apt` e o `dnf` substituem a versão anterior.

## Desinstalar

```bash
sudo apt remove warp-menu      # Ubuntu / Debian
sudo dnf remove warp-menu      # Fedora / RHEL / Rocky
```

## Problemas comuns

| Sintoma | Solução |
|---|---|
| `warp-menu: command not found` logo após instalar | Abra um terminal novo, ou rode `hash -r`. |
| O ícone não aparece no menu | Saia e entre de novo na sessão. Alguns ambientes só releem o menu no login. |
| Status `Unknown` | Confirme que `warp-cli status` funciona no terminal. Se o `warp-cli` estiver em outro local: `WARP_CLI=/caminho/warp-cli warp-menu`. |
| Sem som | Instale o `pipewire-alsa` (Ubuntu/Fedora com PipeWire) e confira se `aplay -l` lista dispositivos. |
| A janela não abre | Rode `WARP_DEBUG=1 warp-menu` no terminal e veja a mensagem de erro. O app precisa de OpenGL 3.3. |
