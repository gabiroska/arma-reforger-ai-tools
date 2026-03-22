/*
 * AI_Tools - AIMapGroups.c
 *
 * Exibe no mapa marcadores de todos os grupos de IA aliados,
 * independente de estarem sob controle do jogador ou não.
 * Um marcador por grupo, posicionado no centróide dos membros vivos.
 *
 * --- ESTRUTURA ---
 *   modded SCR_AIWorld   - Captura todos os agentes via AddedAIAgent/RemovingAIAgent
 *   AIGroupMarkerData    - Dados de rastreamento por grupo (referência AIGroup + marker)
 *   AIMapGroups          - Gerencia os marcadores no mapa para todos os grupos aliados
 *
 * --- DISTINÇÃO VISUAL ---
 *   Cor da fação + ícone LIGHT          = slave group do jogador (unidades comandadas)
 *   Cor da fação + ícone INFANTRY  = demais grupos aliados no mapa
 */

//=============================================================================
// modded SCR_AIWorld
// Mantém lista global de todos os agentes de IA ativos.
// Grupos do editor usam AIGroup base (não SCR_AIGroup), então não podemos
// depender do SCR_GroupsManagerComponent.
//=============================================================================
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

	static void GetAllActiveAgents(out array<AIAgent> outAgents)
	{
		outAgents.Copy(s_aAllAgents);
	}
}

//=============================================================================
// AIGroupMarkerData
// Armazena o rastreamento de um único grupo de IA.
//=============================================================================
class AIGroupMarkerData
{
	AIGroup                 m_Group;
	vector                  m_vLastPosition;
	SCR_MapMarkerBase       m_Marker;
	EMilitarySymbolIdentity m_eIdentity;  // armazenado para uso no RefreshMarker
	bool                    m_bIsSlaveGroup; // distingue ícone UNIT vs INFANTRY

	void AIGroupMarkerData(AIGroup group, vector pos, EMilitarySymbolIdentity identity, bool isSlaveGroup)
	{
		m_Group          = group;
		m_vLastPosition  = pos;
		m_eIdentity      = identity;
		m_bIsSlaveGroup  = isSlaveGroup;
	}
}

//=============================================================================
// AIMapGroups
//=============================================================================
class AIMapGroups
{
	protected static const float POSITION_THRESHOLD = 20.0;
	protected static const float UPDATE_MS          = 2000;

	protected ref array<ref AIGroupMarkerData> m_aGroups = new array<ref AIGroupMarkerData>();

	//-------------------------------------------------------------------------
	void AIMapGroups()
	{
		GetGame().GetCallqueue().CallLater(Tick, UPDATE_MS, true);
		Tick();
	}

	//-------------------------------------------------------------------------
	void ~AIMapGroups()
	{
		GetGame().GetCallqueue().Remove(Tick);
		ClearAllMarkers();
	}

	//=========================================================================
	// Tick
	//=========================================================================
	protected void Tick()
	{
		Faction playerFaction = GetPlayerFaction();
		if (!playerFaction)
			return;

		string playerFactionKey = playerFaction.GetFactionKey();

		// Slave group do jogador — identidade BLUFOR (sólido)
		AIGroup playerSlaveGroup = GetPlayerSlaveGroup();

		// ── Passo 1: coletar agentes e agrupá-los por referência AIGroup ──
		array<AIAgent> allAgents = {};
		SCR_AIWorld.GetAllActiveAgents(allAgents);

		array<AIGroup>           groupRefs = {};
		array<ref array<vector>> groupPos  = {};

		foreach (AIAgent agent : allAgents)
		{
			if (!agent)
				continue;

			AIGroup rawGroup = agent.GetParentGroup();
			if (!rawGroup)
				continue;

			IEntity entity = agent.GetControlledEntity();
			if (!entity)
				continue;

			// Fação via componente da entidade
			FactionAffiliationComponent fComp = FactionAffiliationComponent.Cast(
				entity.FindComponent(FactionAffiliationComponent)
			);
			if (!fComp)
				continue;

			Faction agentFaction = fComp.GetAffiliatedFaction();
			if (!agentFaction || agentFaction.GetFactionKey() != playerFactionKey)
				continue;

			// Filtra mortos
			SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(
				entity.FindComponent(SCR_CharacterControllerComponent)
			);
			if (charCtrl && charCtrl.IsDead())
				continue;

			// Registra agente no grupo
			int idx = groupRefs.Find(rawGroup);
			if (idx < 0)
			{
				groupRefs.Insert(rawGroup);
				groupPos.Insert(new array<vector>());
				idx = groupRefs.Count() - 1;
			}
			groupPos[idx].Insert(entity.GetOrigin());
		}

		// ── Passo 2: criar/atualizar markers ──
		array<AIGroup> activeGroups = {};

		for (int g = 0; g < groupRefs.Count(); g++)
		{
			array<vector> positions = groupPos[g];
			if (positions.IsEmpty())
				continue;

			AIGroup grp = groupRefs[g];
			activeGroups.Insert(grp);

			// Ambos os tipos usam a cor da fação do jogador.
			// A distinção é feita pelo ícone: UNIT (slave) vs INFANTRY (demais).
			EMilitarySymbolIdentity identity = ResolveIdentityForFaction(playerFaction);
			bool isSlaveGroup = (playerSlaveGroup && grp == playerSlaveGroup);

			// Centróide
			vector sum = vector.Zero;
			foreach (vector pos : positions)
				sum = sum + pos;
			vector center = sum * (1.0 / positions.Count());

			// Procura dados existentes
			AIGroupMarkerData existingData;
			foreach (AIGroupMarkerData d : m_aGroups)
			{
				if (d.m_Group == grp)
				{
					existingData = d;
					break;
				}
			}

			if (existingData)
			{
				// Atualiza se identidade, tipo (slave/aliado) ou posição mudaram
				bool identityChanged  = existingData.m_eIdentity    != identity;
				bool slaveChanged     = existingData.m_bIsSlaveGroup != isSlaveGroup;
				bool moved            = vector.Distance(existingData.m_vLastPosition, center) > POSITION_THRESHOLD;

				if (identityChanged || slaveChanged || moved)
				{
					existingData.m_eIdentity     = identity;
					existingData.m_bIsSlaveGroup = isSlaveGroup;
					existingData.m_vLastPosition = center;
					RefreshMarker(existingData, center);
				}
			}
			else
			{
				ref AIGroupMarkerData newData = new AIGroupMarkerData(grp, center, identity, isSlaveGroup);
				m_aGroups.Insert(newData);
				CreateMarkerForGroup(newData, center);
			}
		}

		// ── Passo 3: remover markers de grupos dissolvidos ──
		for (int i = m_aGroups.Count() - 1; i >= 0; i--)
		{
			if (!activeGroups.Contains(m_aGroups[i].m_Group))
			{
				DestroyMarker(m_aGroups[i]);
				m_aGroups.Remove(i);
			}
		}
	}

	//=========================================================================
	// Retorna a fação atual do jogador local
	//=========================================================================
	protected Faction GetPlayerFaction()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;

		IEntity controlled = pc.GetControlledEntity();
		if (!controlled)
			return null;

		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(
			controlled.FindComponent(FactionAffiliationComponent)
		);
		if (!factionComp)
			return null;

		return factionComp.GetAffiliatedFaction();
	}

	//=========================================================================
	// Retorna o slave group do jogador (contém as unidades sob seu comando)
	//=========================================================================
	protected AIGroup GetPlayerSlaveGroup()
	{
		SCR_PlayerControllerGroupComponent groupComp =
			SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
		if (!groupComp)
			return null;

		SCR_AIGroup playersGroup = groupComp.GetPlayersGroup();
		if (!playersGroup)
			return null;

		return playersGroup.GetSlave();
	}

	//=========================================================================
	// Mapeia a fação do jogador para a identidade militar correta do marcador.
	// Suporta BLUFOR, OPFOR e INDFOR; fallback para UNKNOWN.
	//=========================================================================
	protected EMilitarySymbolIdentity ResolveIdentityForFaction(Faction faction)
	{
		if (!faction)
			return EMilitarySymbolIdentity.UNKNOWN;

		string key = faction.GetFactionKey();

		// Mapeamento de faction key → identidade militar
		// Adicione novas entradas conforme necessário para outros mods/missões
		if (key == "US" || key == "US_Army" || key == "NATO" || key == "BLUFOR")
			return EMilitarySymbolIdentity.BLUFOR;

		if (key == "USSR" || key == "Soviet" || key == "OPFOR" || key == "RU")
			return EMilitarySymbolIdentity.OPFOR;

		if (key == "FIA" || key == "INDFOR" || key == "Guerrilla" || key == "Resistance")
			return EMilitarySymbolIdentity.INDFOR;

		return EMilitarySymbolIdentity.UNKNOWN;
	}

	//=========================================================================
	// Resolve o nome de exibição do grupo.
	// Tenta SCR_AIGroup.GetCustomName() e GetCallsigns() antes do fallback.
	//=========================================================================
	protected string ResolveGroupName(AIGroup rawGroup, EMilitarySymbolIdentity identity)
	{
		SCR_AIGroup scrGroup = SCR_AIGroup.Cast(rawGroup);
		if (scrGroup)
		{
			// Nome customizado definido pelo editor de missão
			string customName = scrGroup.GetCustomName();
			if (customName != string.Empty)
				return customName;

			// Callsign no formato "Company-Platoon-Squad"
			// GetCallsigns retorna chaves de localização (#AR-Callsign_...) — traduz antes de usar
			string company, platoon, squad, character, fmt;
			scrGroup.GetCallsigns(company, platoon, squad, character, fmt);
			if (company.StartsWith("#"))  company = WidgetManager.Translate(company);
			if (platoon.StartsWith("#"))  platoon = WidgetManager.Translate(platoon);
			if (squad.StartsWith("#"))    squad    = WidgetManager.Translate(squad);
			if (company != string.Empty || platoon != string.Empty || squad != string.Empty)
			{
				string callsign = company;
				if (platoon != string.Empty)
					callsign = callsign + "-" + platoon;
				if (squad != string.Empty)
					callsign = callsign + "-" + squad;
				return callsign;
			}
		}

		// Fallback baseado na identidade
		if (identity == EMilitarySymbolIdentity.BLUFOR)
			return "Command";

		return string.Empty; // sem label para grupos sem nome definido
	}

	//=========================================================================
	// Marker management
	//=========================================================================

	//-------------------------------------------------------------------------
	protected SCR_MapMarkerManagerComponent GetMarkerManager()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;

		return SCR_MapMarkerManagerComponent.Cast(
			gameMode.FindComponent(SCR_MapMarkerManagerComponent)
		);
	}

	//-------------------------------------------------------------------------
	// Cria um marcador militar no centróide do grupo.
	// Slave group → ícone UNIT | Demais aliados → ícone INFANTRY
	// Ambos usam a cor da fação do jogador.
	//-------------------------------------------------------------------------
	protected void CreateMarkerForGroup(AIGroupMarkerData data, vector worldPos)
	{
		SCR_MapMarkerManagerComponent markerMgr = GetMarkerManager();
		if (!markerMgr)
			return;

		// Slave group usa LIGHT para destacar visualmente; demais aliados usam INFANTRY
		EMilitarySymbolIcon icon;
		if (data.m_bIsSlaveGroup)
			icon = EMilitarySymbolIcon.LIGHT;
		else
			icon = EMilitarySymbolIcon.INFANTRY;

		SCR_MapMarkerBase marker = markerMgr.PrepareMilitaryMarker(
			data.m_eIdentity,
			EMilitarySymbolDimension.LAND,
			icon
		);
		if (!marker)
		{
			// Identidade pode não estar na config — tenta fallback com BLUFOR
			if (data.m_eIdentity != EMilitarySymbolIdentity.BLUFOR)
			{
				marker = markerMgr.PrepareMilitaryMarker(
					EMilitarySymbolIdentity.BLUFOR,
					EMilitarySymbolDimension.LAND,
					icon
				);
			}
			if (!marker)
				return;
		}

		marker.SetWorldPos((int)worldPos[0], (int)worldPos[2]);

		// Nome do grupo como label do marker
		// Slave group recebe label fixo; demais usam callsign/nome do grupo
		string label;
		if (data.m_bIsSlaveGroup)
			label = "AI Units";
		else
			label = ResolveGroupName(data.m_Group, data.m_eIdentity);
		if (label != string.Empty)
			marker.SetCustomText(label);

		// isLocal = true: visível apenas para o jogador local
		markerMgr.InsertStaticMarker(marker, true);
		data.m_Marker = marker;
	}

	//-------------------------------------------------------------------------
	protected void RefreshMarker(AIGroupMarkerData data, vector worldPos)
	{
		DestroyMarker(data);
		CreateMarkerForGroup(data, worldPos);
	}

	//-------------------------------------------------------------------------
	protected void DestroyMarker(AIGroupMarkerData data)
	{
		if (!data.m_Marker)
			return;

		SCR_MapMarkerManagerComponent markerMgr = GetMarkerManager();
		if (markerMgr)
			markerMgr.RemoveStaticMarker(data.m_Marker);

		data.m_Marker = null;
	}

	//-------------------------------------------------------------------------
	protected void ClearAllMarkers()
	{
		SCR_MapMarkerManagerComponent markerMgr = GetMarkerManager();
		foreach (AIGroupMarkerData data : m_aGroups)
		{
			if (data.m_Marker && markerMgr)
				markerMgr.RemoveStaticMarker(data.m_Marker);
		}
		m_aGroups.Clear();
	}
}
