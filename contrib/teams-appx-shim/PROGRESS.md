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

## Trabalhar noutra máquina — o que é preciso

Este repositório (o branch `teams-native-composition`) contém **todo o
código-fonte necessário**: os ficheiros `dlls/*`/`include/*` do Wine em si, e
este diretório inteiro com as fontes do shim (`main.c`, `package.c`,
`pkginfo.c`, `composition.c`, `miniz*`, os `.reg`, `build.sh`, `deploy.sh`,
`test-harness.sh`). Não é preciso copiar nada manualmente de outra máquina —
**exceto** um item, deliberadamente fora do git:

- **`embedded-packages/`** — pacotes `.msix` reais da Microsoft (Teams,
  Windows App Runtime) usados como fallback de staging em `pkginfo.c`. Não
  vão para o git (direitos de autor + ~63MB). Numa máquina nova é preciso
  obter esses `.msix` por fora (instalador oficial do Teams/WebView2/Windows
  App SDK) e colocá-los em `contrib/teams-appx-shim/embedded-packages/` antes
  de instalar — ver `pkginfo.c` para o nome/ordem exato que cada função
  espera.

Passos numa máquina nova (assumindo um `WINEPREFIX` Wine-Staging 11.16 já
criado, com Teams "instalado" via Lutris/instalador MSIX da forma habitual):

```sh
sudo apt install wine-staging gcc-mingw-w64-i686 gcc-mingw-w64-x86-64   # toolchain para winegcc -b
cd contrib/teams-appx-shim
./build.sh                     # compila appxdeploymentclient.dll (64-bit) e appxdeploymentclient32.dll (32-bit)
./deploy.sh [caminho-do-prefix]  # copia para system32/syswow64 + importa todos os register_*.reg
                                  # (por omissão usa $HOME/.local/share/lutris/teams)
./test-harness.sh run 90       # corre o Teams com limpeza correta do wineserver
```

`build.sh`/`deploy.sh` foram escritos e verificados nesta sessão — antes
disto **não existia nenhum script de build funcional** (o `Makefile.in`
neste diretório está desatualizado: não lista `composition.c` nem
`pkginfo.c`, e referencia um `classes.idl` que não é sequer usado — é um
resto do módulo Wine real e separado `dlls/appxdeploymentclient` de Mohamad
Al-Jaf, cujo nome este projeto reaproveita). O comando real (`winegcc -shared
-b x86_64-w64-mingw32/-i686-w64-mingw32 ...`) foi reconstruído e testado do
zero, com sucesso, a partir apenas do que está commitado neste diretório.

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
   `ContainerVisual`, `CompositionBrush`/`CompositionSurfaceBrush`/
   `CompositionColorBrush`, `CompositionTarget`/`DesktopWindowTarget`,
   `VisualCollection`, `CompositionCapabilities`, e a interface base comum
   `CompositionObject` (ver abaixo) — todos com GUIDs verificados contra
   fontes autoritativas (windows-rs tag `74`, WDK IDL mirror `wmliang/wdk-10`),
   nunca adivinhados.
6. **"Blit thread"**: como o Wine não tem DWM/DirectComposition real, foi
   escrita uma thread de fundo que lê o conteúdo da swapchain do Teams via
   `IDXGISwapChain_GetBuffer` + `CopyResource` para uma textura de staging, e
   pinta-o na janela real via GDI (`StretchDIBits`) a ~30fps. É a "ponte" entre
   o render loop do Teams e o ecrã.
7. **Harness de teste limpo** (`test-harness.sh`): elimina falsos "já está a
   correr" causados por mutexes órfãs do wineserver depois de `kill -9`.
   Usar sempre `wineserver -k && wineserver -w` antes de cada teste — ver
   ficheiro para detalhes.

## Ronda seguinte (depois do commit inicial): CompositionCapabilities + CompositionObject + CompositionColorBrush

Retomado o trabalho exatamente onde tinha ficado (ver "O que falta" da versão
anterior deste documento). Três interfaces novas implementadas e verificadas
nesta ronda, cada uma confirmada a avançar o ponto onde o Teams falha (ciclo
"corre com harness limpo → apanha exceção → radare2 + extração de GUID →
implementa → recompila → retesta", exatamente como descrito na secção de
metodologia abaixo):

1. **`ICompositionCapabilities` / `ICompositionCapabilitiesStatics`** —
   implementadas em `composition.c` (classe estática, `GetForCurrentView()`
   devolve uma instância partilhada só com `AreEffectsSupported`/
   `AreEffectsFast` a responder `TRUE`), registadas no dispatcher de
   `main.c` e em `register_compositioncapabilities(_wow).reg`. Confirmado via
   log que `RoGetActivationFactory` para esta classe passa a ter sucesso.
2. **`ICompositionObject` em falta em TODOS os nossos objetos de composição**
   — bug real encontrado por reverse engineering (não estava previsto):
   depois de `CompositionCapabilities` resolver, o Teams continuava a
   crashar com uma exceção C++ (`0xe06d7363`). Backtrace via `winedbg`
   (`bt` depois de `c`, usando um FIFO para injetar comandos só depois da
   exceção acontecer) apontou para um `QueryInterface` CFG-dispatched cujo
   GUID (extraído via o script Python de parsing manual do PE) era
   `bcb4ad45-7609-4550-934f-16002a68fded` = `ICompositionObject` — a
   interface base comum de `Visual`, `CompositionBrush`, `CompositionTarget`
   e `VisualCollection` no Windows real (confirmado no IDL: todas estas
   `runtimeclass` têm `: Windows.UI.Composition.CompositionObject`). Código
   genérico do Teams faz `QueryInterface` a QUALQUER objeto de composição
   para `ICompositionObject` e chama `get_Compositor()` nele — nenhum dos
   nossos objetos respondia a essa interface, logo a QI falhava e o Teams
   lançava uma exceção não tratada. Corrigido adicionando suporte a
   `ICompositionObject` (`QueryInterface`/`AddRef`/`Release` próprios por
   tipo, delegando para o refcount real do objeto; os 5 métodos reais —
   `get_Compositor`, `get_Dispatcher`, `get_Properties`, `StartAnimation`,
   `StopAnimation` — são implementações **partilhadas** entre todos os tipos
   já que nenhum precisa de estado específico do tipo) a `sprite_visual`,
   `container_visual`, `composition_surface_brush`, `composition_target` e
   `visual_collection`. `get_Compositor()` devolve um ponteiro global
   (`g_shared_compositor`, com um AddRef extra para nunca morrer) guardado
   quando o `Compositor` é ativado pela primeira vez — modelo correto para
   este shim porque o Teams só ativa um `Compositor` por processo.
3. **`Compositor::CreateColorBrush`/`CreateColorBrushWithColor` em falta**
   — depois do fix de `ICompositionObject`, novo crash mais profundo no
   mesmo caminho de setup. Desta vez, em vez de extrair o GUID (a chamada
   não era um `QueryInterface`, era uma chamada direta a um slot de vtable),
   comparou-se o offset do slot chamado (`[rax+0x38]` = slot 7) com a ordem
   real dos métodos do `ICompositor` e confirmou-se por eliminação: o slot 6
   (`CreateColorKeyFrameAnimation`) já tinha logging visível e não apareceu
   no log antes do crash, logo só podia ser o slot 7 = `CreateColorBrush`.
   Implementada uma `ICompositionColorBrush` real (GUID
   `2b264c5e-bf35-4831-8642-cf70c20fff2f`, verificado no IDL) com
   `get_Color`/`put_Color`, e ligada aos dois métodos do Compositor.

**Resultado depois destas três correções**: o Teams avança bastante mais no
arranque — chega a pedir `Windows.ApplicationModel.Core.CoreApplication`
(`RoGetActivationFactory` com GUID `0aacf7a4-5e1d-49df-8034-fb6a68bc5ed1`) e
`Microsoft.Graphics.Canvas.CanvasDevice`/`Windows.Foundation.PropertyValue`,
antes de eventualmente crashar outra vez com o mesmo tipo de exceção
(`0xe06d7363`). A falha do `CoreApplication` é logada pelo canal `twinapi`
do **próprio Wine** (`activation_factory_QueryInterface ... not
implemented`) — ou seja, já não é uma lacuna do nosso shim de Composition,
é um gap num componente Wine completamente diferente
(`Windows.ApplicationModel.Core`, não `Windows.UI.Composition`). Parou-se
aqui deliberadamente em vez de perseguir esse novo subsistema — ver
"O que falta" abaixo.

## Bugs reais encontrados e corrigidos na sessão inicial

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

1. **`Windows.ApplicationModel.Core.CoreApplication`** — é a próxima
   `RoGetActivationFactory` que falha (GUID pedido:
   `0aacf7a4-5e1d-49df-8034-fb6a68bc5ed1`), mas desta vez a falha é dentro
   do `twinapi.dll` do **próprio Wine**, não do nosso shim
   `appxdeploymentclient.dll`. Antes de mexer aqui: não é óbvio ainda que
   esta falha específica seja a causa do crash seguinte (há várias outras
   `RoGetActivationFactory`/probes entre ela e a exceção não tratada no log
   — `Microsoft.Graphics.Canvas.CanvasDevice`, `Windows.Foundation.PropertyValue`
   — que também podem ser candidatas). Antes de continuar o ciclo de
   debugging, vale a pena repetir a técnica do backtrace via `winedbg`
   (FIFO + `c` + poll por "Unhandled exception" + `bt`, documentada abaixo)
   para confirmar QUAL das três é mesmo a causa, em vez de assumir que é a
   primeira a aparecer no log.
2. Isto pode ser um projeto maior do que o shim de Composition: implementar
   `Windows.ApplicationModel.Core.CoreApplication` a sério provavelmente
   vive melhor como um patch a `dlls/twinapi.appcore/` (ou onde estiver essa
   classe registada no Wine) do que dentro de `appxdeploymentclient.dll` —
   avaliar isso antes de começar a escrever código.
3. Continuar o ciclo "corre com harness limpo → identifica próxima interface
   em falta pelo mesmo método → implementa → testa" — há seguramente mais
   interfaces WinRT em falta depois desta, dado o tamanho da superfície de
   API que o Teams usa.
4. Confirmar visualmente (via `gnome-screenshot`, não `xwd` — está bloqueado
   pelo Wayland/XWrayland) que a janela do Teams aparece e desenha conteúdo
   real depois de cada correção — ainda não foi feito nesta ronda (o foco
   foi só em avançar o ponto de crash).
5. Eventualmente considerar submeter as correções genéricas do Wine (não
   ligadas ao shim AppX) upstream para o wine-mirror, já que corrigem bugs
   reais não relacionados com Teams especificamente (locale POSIX, ordinais
   shlwapi, `WerGetFlags`, etc.).

### Nota sobre a técnica de backtrace via winedbg (nova nesta ronda)

Além da metodologia já documentada (radare2 + extração manual de GUID),
esta ronda também precisou de apanhar o backtrace COMPLETO no momento exato
da exceção, não só o endereço onde ela acontece. `winedbg` só aceita
comandos via stdin quando está mesmo parado (por omissão fica a correr
livremente depois de lançar o processo, `c` inclusive é preciso mandar
manualmente se ele parar num breakpoint automático inicial no loader). A
forma fiável de o fazer:

```bash
FIFO=/tmp/wdbg.fifo; mkfifo "$FIFO"
( timeout 100 winedbg "C:\\MSTeams\\ms-teams.exe" < "$FIFO" > crash.log 2>&1 ) &
exec 3>"$FIFO"
printf 'c\n' >&3                      # deixa correr
# espera a exceção aparecer no log, só DEPOIS manda o bt:
while ! grep -q "Unhandled exception" crash.log; do sleep 1; done
sleep 1; printf 'bt\nquit\n' >&3
wait
```

Mandar `bt` demasiado cedo (por exemplo, sem esperar por `c` ter feito
efeito) só mostra a stack do loader inicial (`LdrInitializeThunk`), que é
inútil.

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
