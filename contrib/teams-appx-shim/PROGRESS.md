# Microsoft Teams nativo em Wine — estado do projeto

Objetivo: correr o `ms-teams.exe` nativo (Win32 MSIX, não a versão PWA/Electron)
em Wine/Lutris num Ubuntu 22.04, sem tocar em Windows real.

Este documento é o registo de progresso. Não é documentação de arquitetura
definitiva — é um mapa do que já funciona, do que foi tentado, e do que falta,
para retomar o trabalho sem repetir investigação já feita.

## Estrutura deste diretório

Este diretório (`contrib/teams-appx-shim/`) **não faz parte da árvore de fontes
do Wine**. É um projeto standalone que compila para uma DLL (`appxdeploymentclient.dll`)
carregada via `WINEDLLOVERRIDES=appxdeploymentclient=n`, que faz de:

- **AppX/MSIX package manager** (extração, registo de pacotes, staging) — `package.c`, `pkginfo.c`, `main.c`
- **Windows.UI.Composition** (compositor WinRT, visual tree, blit para o ecrã) — `composition.c`
- Descompressão ZIP para extrair os `.msix` — `miniz*.c/h` (biblioteca de terceiros, não modificada)

As alterações ao próprio código-fonte do Wine (fora deste diretório) estão no
resto deste commit/branch, nos ficheiros normais de `dlls/*`.

## O que já funciona (verificado, não suposição)

1. **Instalação MSIX real**: `WindowsAppRuntimeInstall-x64.exe` instala DLLs
   PE32+ genuínas extraídas dos pacotes `.msix` embutidos (não ficheiros vazios —
   isto foi um bug real, corrigido: ver "Bugs corrigidos" abaixo).
2. **Arranque do `ms-teams.exe`**: o processo arranca, sobrevive à
   inicialização, cria os processos WebView2/Chromium (processo browser +
   GPU + renderer), tudo com árvore de processos real.
3. **Janela Win32 real é criada** pelo Teams (confirmado via `xwininfo`).
4. **DXGI/D3D11 render loop real do Teams está a correr** — o Teams está de
   facto a desenhar frames para uma swapchain (`CreateSwapChainForComposition`),
   só não havia forma de essas frames chegarem ao ecrã (ver secção seguinte).
5. **Implementação própria de `Windows.UI.Composition`** (Wine não tem
   suporte nativo nenhum a isto): `Compositor`, `Visual`/`SpriteVisual`/
   `ContainerVisual`, `CompositionBrush`/`CompositionSurfaceBrush`,
   `CompositionTarget`/`DesktopWindowTarget`, `VisualCollection` — todos com
   GUIDs verificados contra fontes autoritativas (windows-rs tag `74`, WDK IDL
   mirror `wmliang/wdk-10`), nunca adivinhados.
6. **"Blit thread"**: como o Wine não tem DWM/DirectComposition real, foi
   escrita uma thread de fundo que lê o conteúdo da swapchain do Teams via
   `IDXGISwapChain_GetBuffer` + `CopyResource` para uma textura de staging, e
   pinta-o na janela real via GDI (`StretchDIBits`) a ~30fps. É a "ponte" entre
   o render loop do Teams e o ecrã.
7. **Harness de teste limpo** (`test-harness.sh`): elimina falsos "já está a
   correr" causados por mutexes órfãs do wineserver depois de `kill -9`.
   Usar sempre `wineserver -k && wineserver -w` antes de cada teste — ver
   ficheiro para detalhes.

## Bugs reais encontrados e corrigidos nesta sessão

Cada um foi individualmente reproduzido, corrigido, testado:

- **Extração MSIX criava diretórios vazios em vez de ficheiros** —
  `MakeDirsRecursive()` estava a ser chamada sobre o caminho completo do
  ficheiro (incluindo o nome do ficheiro), criando um diretório com esse nome
  em vez de criar os diretórios pai e o ficheiro dentro. Corrigido com
  `MakeParentDirs()`.
- **Locale POSIX (`en_US.UTF-8`) não reconhecido** — `find_lcname_entry()` em
  `dlls/kernelbase/locale.c` não sabia lidar com o sufixo `.codeset` no fim do
  nome da locale; causava uma exceção C++ não apanhada. Corrigido a retirar o
  sufixo antes da comparação.
- **`IDesktopWindowTarget` com vtable errada** — o nosso próprio
  `CreateDesktopWindowTarget` devolvia um objeto com a vtable de
  `ICompositionTarget` (que tem `Root`/`SetRoot`) quando o Teams espera
  `IDesktopWindowTarget` (só tem `IsTopmost` além de `IInspectable`). Isto
  fazia o Teams chamar `IsTopmost()` e na realidade invocar `get_Root()`,
  escrevendo um ponteiro numa stack slot do tamanho de um `bool*` —
  corrupção de memória real. Corrigido com uma implementação separada e
  correta de `IDesktopWindowTarget`.
- **`IVisual2` em falta** — interface de "version probe" que o Teams
  consulta via `QueryInterface`; sem ela o Teams assume uma versão mais
  antiga da API e falha mais à frente. GUID confirmado via extração manual
  de bytes do binário `ms-teams.exe` (ver secção de metodologia).
- **`ICompositionSupportsSystemBackdrop` em falta** — idem, probe para
  Mica/Acrylic do Windows 11.
- Várias funções em falta no Wine (não relacionadas com Composition):
  `FindPackagesByPackageFamily`, `WerRegisterAdditionalProcess`,
  `NetGetAadJoinInformation` (HRESULT mal formado), ordinais errados em
  `shlwapi.spec` (`SHGlobalCounterGetValue`/`Increment`/`Decrement` estavam
  em ordinais Wine internos incorretos — corrigidos para os ordinais reais
  632/633/634 confirmados via geoffchappell.com), `WerGetFlags` a devolver
  `E_NOTIMPL` sem inicializar o parâmetro de saída, três funções em
  `windows.security.authentication.onlineid` stub demais para o Teams
  continuar.

## Metodologia usada (para reaplicar se surgirem mais interfaces em falta)

Quando o Teams falha por falta de uma interface COM/WinRT que o Wine não
implementa, o processo que funcionou de forma fiável foi:

1. Correr sob `winedbg` com o harness limpo, apanhar o `Unhandled exception`.
2. Usar **radare2** (`r2 -q -c "s <addr>; af; pdf" ms-teams.exe`) para ver o
   disassembly da função que falha — muito mais rápido que `winedbg` para
   binários grandes.
3. Ter cuidado com **CFG (Control Flow Guard)**: `ms-teams.exe` é compilado
   com CFG, por isso todas as chamadas indiretas passam por um dispatcher
   partilhado (`jmp rax`). Não se pode assumir que duas chamadas para o
   mesmo endereço `call qword [X]` chamam a mesma função — é preciso seguir
   o `mov rax, ...` anterior para saber o alvo real.
2. Extrair o GUID literal referenciado por uma instrução `lea reg,[rip+offset]`
   perto do ponto de falha, usando um pequeno script Python que faz parsing
   manual do PE (RVA → offset de ficheiro via a tabela de secções, lê 16
   bytes como GUID).
3. Procurar esse GUID exato em fontes autoritativas — nunca adivinhar:
   - `https://raw.githubusercontent.com/microsoft/windows-rs/74/...` — o
     branch `master` do windows-rs **removeu** `Windows.UI.Composition` do
     conjunto de features por omissão; a tag `74` (release ~0.6x) ainda o
     tem.
   - WDK IDL mirror `wmliang/wdk-10` para as interfaces mais antigas/base.
4. Implementar a interface com GUID + vtable verificados, registar em
   `main.c`, criar/atualizar `.reg`, recompilar, testar.

## O que falta (próximos passos, por ordem provável)

1. **`ICompositionCapabilities` / `ICompositionCapabilitiesStatics`** —
   já declaradas em `private.h` (GUIDs `8253353e-...` e `f7b7a86e-...`),
   **ainda não implementadas** em `composition.c`, não firificado no
   dispatcher de `main.c`, sem ficheiro `.reg`. Esta era a próxima peça em
   falta identificada antes de se fazer a pausa para arrumar o git.
2. Retomar o ciclo "corre com harness limpo → identifica próxima interface
   em falta pelo mesmo método → implementa → testa" — é muito provável que
   existam mais interfaces WinRT em falta depois de `CompositionCapabilities`,
   dado o tamanho da superfície de API que o Teams usa.
3. Confirmar visualmente (via `gnome-screenshot`, não `xwd` — está bloqueado
   pelo Wayland/XWrayland) que a janela do Teams aparece e desenha conteúdo
   real depois de cada correção.
4. Eventualmente considerar submeter as correções genéricas do Wine (não
   ligadas ao shim AppX) upstream para o wine-mirror, já que corrigem bugs
   reais não relacionados com Teams especificamente (locale POSIX, ordinais
   shlwapi, `WerGetFlags`, etc.).

## Notas sobre este commit

Os ficheiros modificados em `dlls/` e `include/` deste branch foram
originalmente escritos contra uma árvore Wine-Staging 11.16 diferente da
baseline deste fork. Antes de serem copiados para aqui, cada ficheiro foi
comparado linha a linha com o HEAD deste fork para garantir que **nenhuma
funcionalidade já existente no fork foi apagada por engano** por uma cópia
integral de ficheiro — isto aconteceu (e foi corrigido) em pelo menos três
casos: `PackageFullNameFromId` (função inteira tinha sido apagada de
`version.c`), a entrada de export correspondente em `kernel32.spec`/
`kernelbase.spec`/`appmodel.h`, e `RtlpQueryDefaultUILanguage` em
`ntdll.spec`. `locale.c` tinha divergência de baseline grande demais
(funcionalidade de UI language separada do locale) para ser copiado por
inteiro — foi revertido para o HEAD do fork e o fix real (~15 linhas) foi
reaplicado manualmente.
