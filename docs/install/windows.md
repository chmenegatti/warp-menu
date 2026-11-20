# Instalação no Windows

[← Voltar ao README](../../README.md)

Compatível com **Windows 10 e 11, 64 bits**. O app é portátil: não tem instalador, basta extrair o `.zip`.

## 1. Pré-requisito: Cloudflare WARP

O cliente **Cloudflare WARP** precisa estar instalado e registrado na organização da TOTVS (o ícone da nuvem laranja aparece na bandeja do sistema). Para conferir, abra o **Prompt de Comando** e rode:

```
warp-cli status
```

Se o cliente não estiver instalado, siga o [guia da Cloudflare para Windows](https://developers.cloudflare.com/warp-client/get-started/windows/) e as instruções internas da TOTVS para o registro.

O menu acha o `warp-cli` automaticamente em `C:\Program Files\Cloudflare\Cloudflare WARP\`, mesmo que ele não esteja no `PATH`.

## 2. Baixe e extraia

1. Abra a página de [Releases](https://github.com/chmenegatti/warp-menu/releases/latest) e baixe `warp-menu-<versão>-windows-x64.zip`.
2. Clique com o botão direito no arquivo e escolha **Extrair tudo...**.
3. Mova a pasta `TOTVS-WARP` para um lugar fixo. Sugestão: `C:\Users\<seu-usuário>\AppData\Local\Programs\TOTVS-WARP`. Para chegar lá, digite `%LOCALAPPDATA%\Programs` na barra de endereço do Explorador de Arquivos e crie a pasta `Programs` se ela não existir.

A pasta precisa ficar assim. O executável procura a pasta `assets` ao lado dele, então não separe os dois:

```
TOTVS-WARP\
├── warp-menu.exe
└── assets\
```

## 3. Crie um atalho

1. Clique com o botão direito em `warp-menu.exe` → **Mostrar mais opções** (no Windows 11) → **Enviar para** → **Área de trabalho (criar atalho)**.
2. Para colocar no Menu Iniciar, copie esse atalho para `%APPDATA%\Microsoft\Windows\Start Menu\Programs`. Depois disso ele aparece na busca do Iniciar como **warp-menu**, e você pode renomear o atalho para **TOTVS WARP**.

## 4. Primeira execução: aviso do SmartScreen

Como o executável não tem assinatura digital, na primeira vez o Windows pode mostrar **"O Windows protegeu o computador"**:

1. Clique em **Mais informações**.
2. Clique em **Executar assim mesmo**.

O aviso não aparece nas próximas vezes.

## Atualizar

Baixe o `.zip` novo e substitua o conteúdo da pasta `TOTVS-WARP` (feche o app antes). Os atalhos continuam funcionando.

## Desinstalar

Apague a pasta `TOTVS-WARP` e os atalhos que você criou. O app não grava nada no registro do Windows.

## Problemas comuns

| Sintoma | Solução |
|---|---|
| Status `Unknown` | Confirme que o WARP está rodando (ícone na bandeja) e que `warp-cli status` funciona no Prompt de Comando. Se o `warp-cli.exe` estiver em outro lugar, crie a variável de ambiente de usuário `WARP_CLI` com o caminho completo. |
| A janela abre e fecha na hora | Provavelmente falta a pasta `assets` ao lado do `.exe`, ou não há OpenGL 3.3 (comum em máquinas virtuais e via Área de Trabalho Remota). Atualize o driver de vídeo. |
| O antivírus bloqueou o arquivo | Executáveis sem assinatura às vezes são postos em quarentena. Libere o arquivo no antivírus ou fale com o time de TI. |
