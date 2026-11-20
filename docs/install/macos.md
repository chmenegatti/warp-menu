# Instalação no macOS

[← Voltar ao README](../../README.md)

Compatível com **macOS 11 (Big Sur) ou superior**. O pacote é **universal**: roda nativamente em Macs Intel e Apple Silicon (M1, M2, M3, …).

## 1. Pré-requisito: Cloudflare WARP

O cliente **Cloudflare WARP** precisa estar instalado e registrado na organização da TOTVS. Para conferir, abra o **Terminal** e rode:

```bash
warp-cli status
```

Se o cliente não estiver instalado, siga o [guia da Cloudflare para macOS](https://developers.cloudflare.com/warp-client/get-started/macos/) e as instruções internas da TOTVS para o registro.

O menu acha o `warp-cli` automaticamente em `/usr/local/bin` ou dentro de `/Applications/Cloudflare WARP.app`.

## 2. Instale

1. Abra a página de [Releases](https://github.com/chmenegatti/warp-menu/releases/latest) e baixe `warp-menu-<versão>-macos-universal.dmg`.
2. Dê dois cliques no `.dmg`.
3. Arraste **TOTVS WARP** para a pasta **Applications** (Aplicativos) na janela que abrir.
4. Ejete o disco "TOTVS WARP" no Finder.

## 3. Primeira execução: liberar o app

O app não é assinado nem notarizado pela Apple, então na primeira vez o macOS bloqueia a abertura com uma mensagem como *"Apple não pôde verificar se 'TOTVS WARP' está livre de malware"*. Libere de **uma** destas formas:

**Pelos Ajustes do Sistema** (macOS 13 ou superior):

1. Tente abrir o app uma vez e clique em **OK** (ou **Concluído**) no aviso.
2. Abra **Ajustes do Sistema → Privacidade e Segurança**.
3. Role até a seção **Segurança** e clique em **Abrir Mesmo Assim** ao lado da mensagem sobre o TOTVS WARP.
4. Confirme com sua senha e clique em **Abrir**.

**Pelo Terminal** (qualquer versão):

```bash
xattr -dr com.apple.quarantine "/Applications/TOTVS WARP.app"
```

Isso só é necessário na primeira vez e depois de cada atualização.

## 4. Abra o app

Abra **TOTVS WARP** pelo Launchpad, pelo Spotlight (⌘ Espaço → "TOTVS WARP") ou pela pasta Aplicativos. Para deixá-lo no Dock, clique com o botão direito no ícone enquanto ele estiver aberto → **Opções → Manter no Dock**.

## Atualizar

Baixe o `.dmg` novo, arraste o app de novo para **Applications** e confirme **Substituir**. Depois libere a primeira execução outra vez (passo 3).

## Desinstalar

Arraste **TOTVS WARP** de **Applications** para o Lixo. O app não cria outros arquivos.

## Problemas comuns

| Sintoma | Solução |
|---|---|
| "O app está danificado e não pode ser aberto" | É o bloqueio de quarentena. Rode o comando `xattr` do passo 3. |
| Status `Unknown` | Confirme que `warp-cli status` funciona no Terminal. Se o `warp-cli` estiver em outro local, abra o app pelo Terminal: `WARP_CLI=/caminho/warp-cli "/Applications/TOTVS WARP.app/Contents/MacOS/warp-menu"`. |
| A janela não abre | Abra pelo Terminal com `WARP_DEBUG=1 "/Applications/TOTVS WARP.app/Contents/MacOS/warp-menu"` para ver o erro. |
