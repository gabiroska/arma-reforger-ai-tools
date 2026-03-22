# AI_Tools — Notas de Pesquisa de API

Documento de referência técnica. Registra descobertas, limitações e comportamentos confirmados
da API do Arma Reforger / Enfusion Engine relevantes ao desenvolvimento do mod.

---

## Grupos de IA

### Como obter todos os grupos ativos (incluindo grupos do editor)

**Problema:** `SCR_GroupsManagerComponent.GetPlayableGroupsByFaction()` e `GetAllPlayableGroups()`
retornam 0 grupos para grupos colocados diretamente no editor de missão — eles não são "jogáveis".

**Solução confirmada:** `modded SCR_AIWorld` com override de `AddedAIAgent` / `RemovingAIAgent`.
O engine chama esses callbacks para **todo** agente de IA, incluindo os do editor.
Manter um array estático global `s_aAllAgents` populado por esses callbacks é a única forma
confiável de rastrear todos os agentes independente de sua origem.

```c
modded class SCR_AIWorld
{
    protected static ref array<AIAgent> s_aAllAgents = new array<AIAgent>();

    override protected void AddedAIAgent(AIAgent agent)
    {
        super.AddedAIAgent(agent);
        if (agent && !s_aAllAgents.Contains(agent))
            s_aAllAgents.Insert(agent);
    }

    override protected void RemovingAIAgent(AIAgent agent)
    {
        super.RemovingAIAgent(agent);
        if (agent)
            s_aAllAgents.RemoveItem(agent);
    }
}
```

### AIGroup base vs SCR_AIGroup

- Grupos colocados no editor de missão usam a classe base `AIGroup`, **não** `SCR_AIGroup`.
- `SCR_AIGroup.Cast(rawGroup)` retorna `null` para esses grupos.
- Trabalhar sempre com `AIGroup` como tipo base; fazer o cast para `SCR_AIGroup` apenas quando
  precisar de funcionalidades específicas (nome customizado, callsigns).

### Obter a fação de um agente

Não usar `group.GetFaction()` — pode não funcionar para grupos base.
Usar `FactionAffiliationComponent` na entidade controlada pelo agente:

```c
IEntity entity = agent.GetControlledEntity();
FactionAffiliationComponent fComp = FactionAffiliationComponent.Cast(
    entity.FindComponent(FactionAffiliationComponent)
);
Faction faction = fComp.GetAffiliatedFaction();
string key = faction.GetFactionKey();
```

### Faction keys das fações padrão do jogo

| Fação | Key confirmada |
|---|---|
| US Army (BLUFOR) | `"US"` |
| USSR / Soviet (OPFOR) | `"USSR"` |
| FIA / Guerrilha (INDFOR) | `"FIA"` |

> Adicionar novas entradas conforme testado em missões.

### Identificar o slave group do jogador

```c
SCR_PlayerControllerGroupComponent groupComp =
    SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
SCR_AIGroup playersGroup = groupComp.GetPlayersGroup();
AIGroup slaveGroup = playersGroup.GetSlave(); // unidades comandadas pelo jogador
```

### Nome e callsign de grupos

```c
SCR_AIGroup scrGroup = SCR_AIGroup.Cast(rawGroup);
string customName = scrGroup.GetCustomName(); // nome definido no editor

string company, platoon, squad, character, fmt;
scrGroup.GetCallsigns(company, platoon, squad, character, fmt);
```

**Atenção:** `GetCallsigns()` retorna **chaves de localização** no formato `#AR-Callsign_US_Company_A`.
Traduzir antes de exibir:

```c
if (company.StartsWith("#")) company = WidgetManager.Translate(company);
if (platoon.StartsWith("#")) platoon = WidgetManager.Translate(platoon);
if (squad.StartsWith("#"))   squad   = WidgetManager.Translate(squad);
```

---

## Marcadores de Mapa

### API principal

| Classe | Papel |
|---|---|
| `SCR_MapMarkerManagerComponent` | Gerenciador de marcadores, obtido via `gameMode.FindComponent(...)` |
| `SCR_MapMarkerBase` | Instância de marcador individual |

### Fluxo de criação

```c
SCR_MapMarkerBase marker = markerMgr.PrepareMilitaryMarker(
    EMilitarySymbolIdentity.BLUFOR,
    EMilitarySymbolDimension.LAND,
    EMilitarySymbolIcon.INFANTRY
);
marker.SetWorldPos((int)worldPos[0], (int)worldPos[2]); // X e Z (Y é altitude)
marker.SetCustomText("Alpha-1-1");
markerMgr.InsertStaticMarker(marker, true); // true = local only (não replicado)
```

### Remoção

```c
markerMgr.RemoveStaticMarker(marker);
```

### Marcadores clicáveis

**Limitação confirmada:** Não há API pública para tornar marcadores não-clicáveis.
O comportamento de seleção é controlado internamente pelo engine.

### EMilitarySymbolIdentity — valores confirmados

| Valor | Cor | Observação |
|---|---|---|
| `BLUFOR` | Azul | Funciona |
| `OPFOR` | Vermelho | Funciona |
| `INDFOR` | Verde | Funciona |
| `UNKNOWN` | Amarelo/cinza | Funciona |
| `ASSUMED_BLUFOR` | — | **NÃO está na config do jogo** — `PrepareMilitaryMarker` retorna `null` |

### EMilitarySymbolIcon — valores confirmados

Confirmados via autocomplete do Workbench:

| Valor | Descrição |
|---|---|
| `INFANTRY` | Infantaria padrão ✅ |
| `LIGHT` | Infantaria leve ✅ |
| `RECON` | Reconhecimento ✅ |
| `ARMOR` | Blindado ✅ |
| `RELAY` | Retransmissão ✅ |
| `SUPPLY` | Suprimento ✅ |
| `SIGNAL` | Sinal ✅ |
| `SNIPER` | Atirador ✅ |
| `MORTAR` | Morteiro ✅ |
| `UNARMED` | Desarmado ✅ |
| `UNIT` | **NÃO existe** ❌ |
| `HEADQUARTERS` | **NÃO existe** ❌ |

> Lista parcial — há mais valores abaixo de `UNARMED` no autocomplete não capturados.

### EMilitarySymbolDimension — valores conhecidos

| Valor | Descrição |
|---|---|
| `LAND` | Terrestre ✅ |
| `AIR` | Aéreo (não testado) |

---

## Enforce Script — Limitações e Gotchas

### Operador ternário não suportado

```c
// ERRO — não compila:
string s = condition ? "a" : "b";

// CORRETO:
string s;
if (condition) s = "a";
else s = "b";
```

### string.StartsWith()

Disponível e funcional para verificar prefixo de chave de localização:
```c
if (key.StartsWith("#")) key = WidgetManager.Translate(key);
```

### Comparação de referências AIGroup

`AIGroup` não expõe um ID numérico acessível via script quando não castado para `SCR_AIGroup`.
Para deduplicar grupos, usar comparação direta de referência com `Find()` em array:

```c
int idx = groupRefs.Find(rawGroup); // compara por referência
```

---

## Multiplayer — Considerações

- `InsertStaticMarker(marker, true)` — marcador **local only**, não replicado. Jogadores inimigos
  não podem ver marcadores de outros jogadores via rede.
- `modded SCR_AIWorld` com callbacks de agente: em multiplayer dedicado, os callbacks podem não
  disparar no cliente para todos os agentes (depende do nível de replicação do servidor).
  **Funcionalidade garantida apenas em singleplayer e host cooperativo.**
- `GetPlayerController()` e `GetLocalPlayerControllerGroupComponent()` sempre retornam dados
  do jogador **local** — seguro para uso em multiplayer.

---

## Performance

- Tick a cada 2000ms (`UPDATE_MS`) minimiza impacto de CPU.
- `s_aAllAgents.Contains(agent)` em `AddedAIAgent` é O(n) — pode gerar pico em spawns em massa.
- Dentro do tick: dois `FindComponent` por agente + O(n) lookup por grupo.
  Aceitável para missões com até ~100 unidades.
