/*
 * AI_Tools - AISquadHUD.c
 *
 * Exibe um painel HUD com os membros do grupo do jogador.
 * Funciona em qualquer modo de jogo automaticamente.
 *
 * --- ESTRUTURA DO CÓDIGO ---
 *   AISquadEntry        - Uma linha na lista (representa uma unidade)
 *   AISquadHUD          - O painel completo com título + lista
 *   modded SCR_PlayerController - Hook de spawn/morte do jogador
 */

//=============================================================================
// AISquadEntry
// Cada unidade é um FrameWidget posicionado absolutamente no painel pai,
// com fundo escuro próprio e espaço entre as linhas.
//=============================================================================
class AISquadEntry
{
	protected static const ResourceName GAME_FONT = "{D3E75BCBECEB273E}UI/Fonts/NotoSans/NotoSans-Enhanced-Regular-JP-KOR-SC-LAT.fnt";

	// Duração da animação de morte: 6 ticks × 500ms = 3 segundos
	protected static const int DEATH_DELAY_TICKS = 6;

	IEntity        m_Entity;
	Widget         m_wRow;  // FrameWidget — posicionado via FrameSlot pelo AISquadHUD
	TextWidget     m_wBar;  // Referência ao marcador "|" para alterar cor na morte
	bool           m_bDying = false;
	protected int  m_iDyingTicksLeft = 0;

	//-------------------------------------------------------------------------
	void AISquadEntry(IEntity entity, Widget parent, float rowWidth, float rowHeight)
	{
		m_Entity = entity;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !parent)
			return;

		// Frame da linha — posição Y definida depois via SetRowY()
		m_wRow = workspace.CreateWidget(
			WidgetType.FrameWidgetTypeID,
			WidgetFlags.VISIBLE,
			Color.White,
			0,
			parent
		);
		if (!m_wRow)
			return;

		FrameSlot.SetAnchorMin(m_wRow, 0, 0);
		FrameSlot.SetAnchorMax(m_wRow, 0, 0);
		FrameSlot.SetPos(m_wRow, 0, 0);
		FrameSlot.SetSize(m_wRow, rowWidth, rowHeight);

		// Fundo escuro individual desta linha
		Widget rowBg = workspace.CreateWidget(
			WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.STRETCH,
			Color.FromRGBA(0, 0, 0, 120),
			0,
			m_wRow
		);
		if (rowBg)
		{
			FrameSlot.SetAnchorMin(rowBg, 0, 0);
			FrameSlot.SetAnchorMax(rowBg, 1, 1);
			FrameSlot.SetOffsets(rowBg, 0, 0, 0, 0);
		}

		// Conteúdo horizontal: [marcador] [nome] — sort=1, acima do fundo
		HorizontalLayoutWidget rowContent = HorizontalLayoutWidget.Cast(
			workspace.CreateWidget(
				WidgetType.HorizontalLayoutWidgetTypeID,
				WidgetFlags.VISIBLE,
				Color.White,
				1,
				m_wRow
			)
		);
		if (!rowContent)
			return;

		FrameSlot.SetAnchorMin(rowContent, 0, 0);
		FrameSlot.SetAnchorMax(rowContent, 1, 1);
		FrameSlot.SetOffsets(rowContent, 0, 0, 0, 0);

		// Marcador verde "|" — salvo em m_wBar para alterar cor ao morrer
		m_wBar = TextWidget.Cast(
			workspace.CreateWidget(
				WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE,
				Color.FromRGBA(92, 192, 100, 255),
				0,
				rowContent
			)
		);
		if (m_wBar)
		{
			m_wBar.SetText("|");
			m_wBar.SetExactFontSize(15);
			m_wBar.SetBold(true);
			m_wBar.SetFont(GAME_FONT);
			HorizontalLayoutSlot.SetPadding(m_wBar, 5, 0, 4, 0);
			HorizontalLayoutSlot.SetVerticalAlign(m_wBar, LayoutVerticalAlign.Center);
		}

		// Nome da unidade
		TextWidget nameText = TextWidget.Cast(
			workspace.CreateWidget(
				WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE,
				Color.FromRGBA(230, 235, 230, 255),
				0,
				rowContent
			)
		);
		if (nameText)
		{
			nameText.SetText(ResolveUnitName(entity));
			nameText.SetExactFontSize(13);
			nameText.SetFont(GAME_FONT);
			HorizontalLayoutSlot.SetPadding(nameText, 0, 0, 8, 0);
			HorizontalLayoutSlot.SetVerticalAlign(nameText, LayoutVerticalAlign.Center);
		}
	}

	//-------------------------------------------------------------------------
	// Reposiciona a linha verticalmente no painel pai
	void SetRowY(float y)
	{
		if (m_wRow)
			FrameSlot.SetPos(m_wRow, 0, y);
	}

	//-------------------------------------------------------------------------
	// Inicia a animação de morte: marcador vira vermelho e inicia contagem
	void MarkAsDying()
	{
		if (m_bDying)
			return;

		m_bDying = true;
		m_iDyingTicksLeft = DEATH_DELAY_TICKS;

		if (m_wBar)
			m_wBar.SetColor(Color.FromRGBA(210, 55, 55, 255));
	}

	//-------------------------------------------------------------------------
	// Decrementa o contador de morte. Retorna true quando pronto para remover.
	bool TickDying()
	{
		m_iDyingTicksLeft--;
		return m_iDyingTicksLeft <= 0;
	}

	//-------------------------------------------------------------------------
	void Remove()
	{
		if (m_wRow)
		{
			m_wRow.RemoveFromHierarchy();
			m_wRow = null;
		}
	}

	//-------------------------------------------------------------------------
	protected string ResolveUnitName(IEntity entity)
	{
		if (!entity)
			return "???";

		SCR_CharacterIdentityComponent identityComp = SCR_CharacterIdentityComponent.Cast(
			entity.FindComponent(SCR_CharacterIdentityComponent)
		);
		if (identityComp)
		{
			string format, name, alias, surname;
			identityComp.GetFormattedFullName(format, name, alias, surname);

			if (name != string.Empty || surname != string.Empty)
			{
				string result = name;
				if (alias != string.Empty)
					result = result + " '" + alias + "'";
				if (surname != string.Empty)
					result = result + " " + surname;
				return result;
			}
		}

		string entityName = entity.GetName();
		if (entityName != string.Empty)
			return entityName;

		return entity.ClassName();
	}
}

//=============================================================================
// AISquadHUD
//=============================================================================
class AISquadHUD
{
	// ── Configurações ──────────────────────────────────────────────────────
	protected static const float PANEL_POS_X   = 10;
	protected static const float PANEL_POS_Y   = 10;
	protected static const float PANEL_WIDTH   = 220;
	protected static const float HEADER_HEIGHT = 26; // altura da linha de título
	protected static const float ROW_HEIGHT    = 22; // altura de cada linha de unidade
	protected static const float ROW_GAP       = 3;  // espaço entre linhas
	protected static const float UPDATE_MS     = 500;

	protected static const ResourceName GAME_FONT = "{D3E75BCBECEB273E}UI/Fonts/NotoSans/NotoSans-Enhanced-Regular-JP-KOR-SC-LAT.fnt";

	// ── Widgets ────────────────────────────────────────────────────────────
	protected Widget     m_wRoot;
	protected TextWidget m_wTitle;

	// ── Estado ────────────────────────────────────────────────────────────
	protected ref map<IEntity, ref AISquadEntry> m_mEntries = new map<IEntity, ref AISquadEntry>();
	protected SCR_AIGroup m_TrackedGroup;
	protected SCR_AIGroup m_TrackedSlaveGroup;

	//-------------------------------------------------------------------------
	void AISquadHUD()
	{
		BuildPanel();

		if (!m_wRoot)
			return;

		m_wRoot.SetVisible(false);

		GetGame().GetCallqueue().CallLater(Tick, UPDATE_MS, true);
		SubscribeToGroupEvents();
		Tick();
	}

	//-------------------------------------------------------------------------
	void ~AISquadHUD()
	{
		GetGame().GetCallqueue().Remove(Tick);
		UnsubscribeFromGroupEvents();

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}
	}

	//=========================================================================
	// Eventos de grupo
	//=========================================================================
	protected void OnGroupChanged(int groupId)
	{
		UnsubscribeFromAgentEvents();

		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (gm)
			m_TrackedGroup = gm.FindGroup(groupId);

		SubscribeToAgentEvents();
		Tick();
	}

	protected void OnAgentAdded(AIAgent agent)   { Tick(); }
	protected void OnAgentRemoved(AIAgent agent) { Tick(); }

	protected void SubscribeToGroupEvents()
	{
		SCR_PlayerControllerGroupComponent groupComp = GetLocalGroupComponent();
		if (!groupComp)
			return;

		groupComp.GetOnGroupChanged().Insert(OnGroupChanged);
		m_TrackedGroup = groupComp.GetPlayersGroup();
		SubscribeToAgentEvents();
	}

	protected void UnsubscribeFromGroupEvents()
	{
		SCR_PlayerControllerGroupComponent groupComp = GetLocalGroupComponent();
		if (groupComp)
			groupComp.GetOnGroupChanged().Remove(OnGroupChanged);

		UnsubscribeFromAgentEvents();

		if (m_TrackedSlaveGroup)
		{
			m_TrackedSlaveGroup.GetOnAgentAdded().Remove(OnAgentAdded);
			m_TrackedSlaveGroup.GetOnAgentRemoved().Remove(OnAgentRemoved);
			m_TrackedSlaveGroup = null;
		}
	}

	protected void SubscribeToAgentEvents()
	{
		if (!m_TrackedGroup)
			return;

		m_TrackedGroup.GetOnAgentAdded().Insert(OnAgentAdded);
		m_TrackedGroup.GetOnAgentRemoved().Insert(OnAgentRemoved);
	}

	protected void UnsubscribeFromAgentEvents()
	{
		if (!m_TrackedGroup)
			return;

		m_TrackedGroup.GetOnAgentAdded().Remove(OnAgentAdded);
		m_TrackedGroup.GetOnAgentRemoved().Remove(OnAgentRemoved);
		m_TrackedGroup = null;
	}

	protected SCR_PlayerControllerGroupComponent GetLocalGroupComponent()
	{
		return SCR_PlayerControllerGroupComponent.GetLocalPlayerControllerGroupComponent();
	}

	//=========================================================================
	// BuildPanel
	// Cria apenas o painel raiz e a linha de título.
	// As linhas de unidades são adicionadas dinamicamente em Tick().
	//=========================================================================
	protected void BuildPanel()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		// Painel raiz — altura ajustada dinamicamente por RepositionRows()
		m_wRoot = workspace.CreateWidget(
			WidgetType.FrameWidgetTypeID,
			WidgetFlags.VISIBLE,
			Color.White,
			0,
			workspace
		);
		if (!m_wRoot)
			return;

		FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
		FrameSlot.SetAnchorMax(m_wRoot, 0, 0);
		FrameSlot.SetPos(m_wRoot, PANEL_POS_X, PANEL_POS_Y);
		FrameSlot.SetSize(m_wRoot, PANEL_WIDTH, HEADER_HEIGHT);

		// Linha do título — FrameWidget fixo no topo
		Widget titleFrame = workspace.CreateWidget(
			WidgetType.FrameWidgetTypeID,
			WidgetFlags.VISIBLE,
			Color.White,
			0,
			m_wRoot
		);
		if (!titleFrame)
			return;

		FrameSlot.SetAnchorMin(titleFrame, 0, 0);
		FrameSlot.SetAnchorMax(titleFrame, 0, 0);
		FrameSlot.SetPos(titleFrame, 0, 0);
		FrameSlot.SetSize(titleFrame, PANEL_WIDTH, HEADER_HEIGHT);

		// Fundo escuro da linha de título
		Widget titleBg = workspace.CreateWidget(
			WidgetType.ImageWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.STRETCH,
			Color.FromRGBA(0, 0, 0, 120),
			0,
			titleFrame
		);
		if (titleBg)
		{
			FrameSlot.SetAnchorMin(titleBg, 0, 0);
			FrameSlot.SetAnchorMax(titleBg, 1, 1);
			FrameSlot.SetOffsets(titleBg, 0, 0, 0, 0);
		}

		// Texto do título — sort=1, acima do fundo
		m_wTitle = TextWidget.Cast(
			workspace.CreateWidget(
				WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE,
				Color.FromRGBA(92, 192, 100, 255),
				1,
				titleFrame
			)
		);
		if (m_wTitle)
		{
			m_wTitle.SetText("Units");
			m_wTitle.SetExactFontSize(12);
			m_wTitle.SetBold(true);
			m_wTitle.SetFont(GAME_FONT);
			FrameSlot.SetAnchorMin(m_wTitle, 0, 0);
			FrameSlot.SetAnchorMax(m_wTitle, 1, 1);
			FrameSlot.SetOffsets(m_wTitle, 8, 7, 0, 7);
		}
	}

	//=========================================================================
	// RepositionRows
	// Reposiciona todas as linhas e ajusta a altura do painel ao conteúdo.
	//=========================================================================
	protected void RepositionRows()
	{
		int i = 0;
		foreach (IEntity e, AISquadEntry entry : m_mEntries)
		{
			float y = HEADER_HEIGHT + ROW_GAP + i * (ROW_HEIGHT + ROW_GAP);
			entry.SetRowY(y);
			i++;
		}

		int count = m_mEntries.Count();
		float totalHeight;
		if (count > 0)
			totalHeight = HEADER_HEIGHT + ROW_GAP + count * (ROW_HEIGHT + ROW_GAP);
		else
			totalHeight = HEADER_HEIGHT;

		if (m_wRoot)
			FrameSlot.SetSize(m_wRoot, PANEL_WIDTH, totalHeight);
	}

	//=========================================================================
	// Tick
	//=========================================================================
	protected void Tick()
	{
		array<IEntity> activeUnits = {};
		CollectGroupMembers(activeUnits);

		// Processa entradas que saíram de activeUnits
		array<IEntity> toRemove = {};
		foreach (IEntity e, AISquadEntry entry : m_mEntries)
		{
			if (activeUnits.Contains(e))
				continue;

			if (entry.m_bDying)
			{
				// Já em animação de morte — decrementa contador
				if (entry.TickDying())
					toRemove.Insert(e);
			}
			else
			{
				// Verifica se morreu ou apenas saiu do grupo
				SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(
					e.FindComponent(SCR_CharacterControllerComponent)
				);
				if (charCtrl && charCtrl.IsDead())
					entry.MarkAsDying(); // morte: animação de 3s antes de remover
				else
					toRemove.Insert(e);  // saiu do grupo: remoção imediata
			}
		}
		foreach (IEntity e : toRemove)
		{
			m_mEntries[e].Remove();
			m_mEntries.Remove(e);
		}

		// Adiciona novas entradas como filhos diretos do painel raiz
		foreach (IEntity e : activeUnits)
		{
			if (!m_mEntries.Contains(e) && m_wRoot)
			{
				ref AISquadEntry entry = new AISquadEntry(e, m_wRoot, PANEL_WIDTH, ROW_HEIGHT);
				m_mEntries.Insert(e, entry);
			}
		}

		int count = activeUnits.Count();
		int totalEntries = m_mEntries.Count(); // inclui entradas em animação de morte

		if (m_wTitle)
			m_wTitle.SetText(string.Format("Units (%1)", count));

		RepositionRows();

		// Painel permanece visível enquanto houver entradas (inclusive morrendo)
		if (m_wRoot)
			m_wRoot.SetVisible(totalEntries > 0);
	}

	//=========================================================================
	// CollectGroupMembers
	//=========================================================================
	protected void CollectGroupMembers(out array<IEntity> units)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;

		SCR_PlayerControllerGroupComponent groupComp = GetLocalGroupComponent();
		if (!groupComp)
			return;

		SCR_AIGroup playerGroup = groupComp.GetPlayersGroup();
		if (!playerGroup)
		{
			SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
			if (gm)
				playerGroup = gm.GetPlayerGroup(pc.GetPlayerId());
		}
		if (!playerGroup)
			return;

		if (m_TrackedGroup != playerGroup)
		{
			UnsubscribeFromAgentEvents();
			m_TrackedGroup = playerGroup;
			SubscribeToAgentEvents();
		}

		IEntity playerEntity = pc.GetControlledEntity();

		array<SCR_ChimeraCharacter> mainMembers = playerGroup.GetAIMembers();
		if (mainMembers)
			CollectAliveFromCharacters(mainMembers, playerEntity, units);

		SCR_AIGroup slaveGroup = playerGroup.GetSlave();
		if (slaveGroup)
		{
			array<SCR_ChimeraCharacter> slaveMembers = slaveGroup.GetAIMembers();
			if (slaveMembers)
				CollectAliveFromCharacters(slaveMembers, playerEntity, units);

			if (m_TrackedSlaveGroup != slaveGroup)
			{
				if (m_TrackedSlaveGroup)
				{
					m_TrackedSlaveGroup.GetOnAgentAdded().Remove(OnAgentAdded);
					m_TrackedSlaveGroup.GetOnAgentRemoved().Remove(OnAgentRemoved);
				}
				m_TrackedSlaveGroup = slaveGroup;
				m_TrackedSlaveGroup.GetOnAgentAdded().Insert(OnAgentAdded);
				m_TrackedSlaveGroup.GetOnAgentRemoved().Insert(OnAgentRemoved);
			}
		}
	}

	//-------------------------------------------------------------------------
	protected void CollectAliveFromCharacters(array<SCR_ChimeraCharacter> characters, IEntity playerEntity, out array<IEntity> units)
	{
		foreach (SCR_ChimeraCharacter character : characters)
		{
			if (!character || character == playerEntity)
				continue;

			SCR_CharacterControllerComponent charCtrl = SCR_CharacterControllerComponent.Cast(
				character.FindComponent(SCR_CharacterControllerComponent)
			);
			if (charCtrl && charCtrl.IsDead())
				continue;

			units.Insert(character);
		}
	}
}

//=============================================================================
// modded class SCR_PlayerController
//=============================================================================
modded class SCR_PlayerController
{
	private ref AISquadHUD   m_AISquadHUD;
	private ref AIMapGroups  m_AIMapGroups;

	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (GetGame().GetPlayerController() != this)
			return;

		if (to)
		{
			if (!m_AISquadHUD)
				m_AISquadHUD = new AISquadHUD();

			if (!m_AIMapGroups)
				m_AIMapGroups = new AIMapGroups();
		}
		else
		{
			m_AISquadHUD  = null;
			m_AIMapGroups = null;
		}
	}
}