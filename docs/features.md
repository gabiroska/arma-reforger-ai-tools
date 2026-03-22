# AI_Tools — Feature Tracker

---

## Implementadas

### Lista de Unidades no HUD

Painel exibido no canto superior esquerdo listando as unidades de IA sob comando do jogador.

- Aparece automaticamente ao recrutar unidades e some quando o grupo fica vazio
- Altura do painel se ajusta dinamicamente à quantidade de unidades
- Cada linha tem fundo escuro individual com espaço entre elas
- Título "Units (N)" com contagem de membros vivos
- Marcador verde `|` ao lado de cada nome
- Fonte nativa do Arma Reforger (NotoSans)
- Funciona em qualquer modo de jogo sem configuração adicional

### Detecção e Animação de Morte

Ao morrer, a unidade exibe uma animação antes de ser removida da lista. Unidades que saem do grupo sem morrer são removidas imediatamente.

- Marcador `|` muda de verde para vermelho ao detectar `IsDead() = true`
- Linha permanece visível por ~3 segundos (6 ticks × 500ms) antes de ser removida
- Painel permanece visível durante a animação, mesmo que não haja mais unidades vivas
- Unidades que saem do grupo (vivas) são removidas imediatamente sem animação
- Contagem no título reflete apenas unidades vivas

### Resolução de Nome da Unidade

Nome exibido usa a identidade do personagem quando disponível.

- Usa `SCR_CharacterIdentityComponent.GetFormattedFullName()` para nome completo
- Fallback para `entity.GetName()` e depois `entity.ClassName()`
- Suporta alias: `"Firstname 'Alias' Surname"`

### Marcadores de Grupos de IA no Mapa

Exibe no mapa um marcador por grupo de IA aliado, posicionado no centróide dos membros vivos.

- Um marcador por grupo (não por unidade), atualizado a cada 2 segundos
- Slave group do jogador: cor da fação + ícone `LIGHT`, label fixo `"AI Units"`
- Demais grupos aliados: cor da fação + ícone `INFANTRY`, label com callsign traduzido (`Alpha-1-1`) ou nome customizado do editor
- Cor automática por fação: BLUFOR (azul), OPFOR (vermelho), INDFOR (verde)
- Marcadores locais — visíveis apenas para o jogador local, não replicados na rede
- Grupos sem membros vivos têm o marcador removido automaticamente
- Funciona para grupos colocados diretamente no editor de missão (não apenas grupos "jogáveis")

---

## A Implementar

---

### Ícones de Role por Unidade

**O que é:**
Exibir um ícone da classe/role da unidade (rifleman, medic, engineer, etc.) ao lado do nome em cada linha.

**Por que:**
Identificar rapidamente a composição do grupo sem precisar ler os nomes.

**Pré-requisitos antes de implementar:**
1. Descobrir todos os roles existentes no jogo — escrever debug temporário que loga os roles únicos das entidades AI durante o jogo. API a investigar: `SCR_EditableEntityComponent`, `SCR_CharacterRoleComponent`.
2. Escolher fonte dos assets de ícone (ver opções abaixo).
3. Mapear cada role para o nome da imagem correspondente no imageset.

**Opções de fonte dos ícones:**
- **Opção A — Imageset vanilla** `{70E828A2F6EBE7D0}UI/Textures/Nametags/nametagicons.imageset`: asset do jogo base, sem dependências. Nomes das imagens disponíveis precisam ser investigados.
- **Opção B — Imageset próprio**: criar PNGs + arquivo `.imageset` importado pelo Workbench. Controle total, mais trabalho.
- **Opção C — Dependência do CSI mod**: reutilizar `{4FE53F33D8545E0D}UI/Textures/HUD/Icons/CSI_ICONS.imageset`. Requer permissão dos autores e declarar o mod como dependência.

**Implementação no script:**
- Adicionar `ImageWidget` por linha em `AISquadEntry`
- Chamar `imageWidget.LoadImageFromSet(0, IMAGESET_PATH, roleIconName)` com o nome do role

---

### Indicador de Estado — Incapacitado

**O que é:**
Em vez de remover imediatamente unidades incapacitadas (inconscientes), exibi-las com o marcador em cor diferente até a morte confirmada.

**Por que:**
Unidades incapacitadas ainda podem ser reanimadas — remover da lista prematuramente perde essa informação tática.

**Estados propostos:**
- Verde `|` — viva
- Amarelo `|` — incapacitada / inconsciente
- Removida — morta confirmada

**Implementação:**
- Manter referência ao `TextWidget` do marcador em `AISquadEntry`
- Verificar `SCR_CharacterControllerComponent.IsUnconscious()` ou equivalente no tick
- Alterar cor do marcador via `bar.SetColor()`

---

### Posição e Tamanho Configuráveis

**O que é:**
Permitir que o jogador ajuste a posição (X, Y) e largura do painel via configuração do mod.

**Por que:**
Usuários com HUDs personalizados podem querer reposicionar o painel para não conflitar com outros elementos.

**Implementação:**
- Expor `PANEL_POS_X`, `PANEL_POS_Y`, `PANEL_WIDTH` como parâmetros de configuração
- Investigar sistema de settings do mod (`SCR_BaseGameMode` ou arquivo de config)
- Aplicar valores em `BuildPanel()` via `FrameSlot.SetPos()` e `FrameSlot.SetSize()`

---

### Ícone de Estado Tático da Unidade

**O que é:**
Exibir um ícone ou indicador ao lado do nome da unidade representando sua ação atual: patrulhando, defendendo, movendo, em combate, dentro de veículo, etc.

**Por que:**
Permite ao jogador entender o estado do grupo de relance, sem precisar observar cada unidade individualmente no campo.

**Estados propostos:**
- Patrulhando
- Defendendo (posição estática)
- Movendo (em deslocamento para waypoint)
- Em combate (engajando inimigo)
- Dentro de veículo
- Incapacitado / inconsciente

**Pré-requisitos antes de implementar:**
- Investigar API de comportamento da IA: `AIAgent`, `SCR_AIActionBase`, `SCR_AISuspectedLocation` ou similar
- Verificar se o estado atual do agente é acessível via `AIAgent.GetCurrentAction()` ou equivalente
- Definir mapeamento de estado → ícone ou símbolo de texto

**Implementação:**
- Adicionar `TextWidget` ou `ImageWidget` de estado em `AISquadEntry`
- Atualizar o estado a cada tick via método `UpdateState()` na entrada
- Usar ícones do imageset (quando implementado) ou caracteres de texto como placeholder

---

### Compatibilidade com Coalition Mod

**O que é:**
Garantir que o AI_Tools funcione corretamente quando carregado junto com o Coalition Squad Interface (CSI) mod, sem conflitos de UI ou de sistemas de grupo.

**Por que:**
O CSI mod também modifica o HUD e o sistema de grupos — podem existir conflitos de eventos, sobreposição de widgets ou comportamento inesperado ao usar os dois mods simultaneamente.

**O que investigar:**
- Verificar se o CSI mod faz `modded` nas mesmas classes que o AI_Tools (`SCR_PlayerController`, `SCR_PlayerControllerGroupComponent`)
- Testar se os eventos de grupo (`OnAgentAdded`, `OnAgentRemoved`) disparam corretamente com os dois mods ativos
- Verificar sobreposição visual de widgets no canto da tela

**Possíveis ações:**
- Adicionar verificação de presença do CSI via `GetGame().IsModLoaded()` ou equivalente e desativar funcionalidades duplicadas
- Coordenar com autores do CSI se necessário para definir uma API de integração