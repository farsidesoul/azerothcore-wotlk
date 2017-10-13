#include "ScriptMgr.h"
#include "Chat.h"
#include "Language.h"

class CustomRates
{
private:
	static int32 GetRateFromDB(const Player *player, CharacterDatabaseStatements statement)
	{
		PreparedStatement *stmt = CharacterDatabase.GetPreparedStatement(statement);
		stmt->setUInt32(0, player->GetGUIDLow());
		PreparedQueryResult result = CharacterDatabase.Query(stmt);

		if (result)
			return static_cast<int32>((*result)[0].GetUInt32());

		return -1;
	}

	static void SaveRateToDB(const Player *player, uint32 rate, bool update, CharacterDatabaseStatements uStmt, CharacterDatabaseStatements iStmt)
	{
		if (update)
		{
			PreparedStatement *stmt = CharacterDatabase.GetPreparedStatement(uStmt);
			stmt->setUInt32(0, rate);
			stmt->setUInt32(1, player->GetGUIDLow());
			CharacterDatabase.Execute(stmt);
		}
		else
		{
			PreparedStatement *stmt = CharacterDatabase.GetPreparedStatement(iStmt);
			stmt->setUInt32(0, player->GetGUIDLow());
			stmt->setUInt32(1, rate);
			CharacterDatabase.Execute(stmt);
		}
	}
public:
	static void DeleteRateFromDB(uint64 guid, CharacterDatabaseStatements statement)
	{
		PreparedStatement *stmt = CharacterDatabase.GetPreparedStatement(statement);
		stmt->setUInt32(0, GUID_LOPART(guid));
		CharacterDatabase.Execute(stmt);
	}

	static int32 GetXpRateFromDB(const Player *player)
	{
		return GetRateFromDB(player, CHAR_SEL_CUSTOM_XP_RATE);
	}

	static void SaveXpRateToDB(const Player *player, uint32 rate, bool update)
	{
		SaveRateToDB(player, rate, update, CHAR_UPD_CUSTOM_XP_RATE, CHAR_INS_CUSTOM_XP_RATE);
	}
};

class add_del_rates : public PlayerScript
{
public:
	add_del_rates() : PlayerScript("add_del_rates")
	{
	}

	void OnDelete(uint64 guid)
	{
		CustomRates::DeleteRateFromDB(guid, CHAR_DEL_CUSTOM_XP_RATE);
	}

	void OnLogin(Player *player)
	{
		int32 rate;

		// Only present xp rates if they are enabled.
		if (sWorld->getBoolConfig(CONFIG_CUSTOM_XP_RATE))
		{
			// show custom XP rate on login
			rate = CustomRates::GetXpRateFromDB(player);

			if (rate != -1 && player->getLevel() != sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
			{
				uint32 uRate = static_cast<uint32>(rate);
				player->SetCustomXPRate(uRate);

				if (sWorld->getBoolConfig(CONFIG_CUSTOM_XP_RATE_SHOW_ON_LOGIN))
				{
					if (uRate)
						ChatHandler(player->GetSession()).PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: Your XP rate was set to %u.", uRate);
					else
						ChatHandler(player->GetSession()).SendSysMessage("|CFF7BBEF7[Custom Rates]|r: Your XP rate was set to 0. You won't gain any XP anymore.");
				}
			}
			else
			{
				uint32 defaultRate = sWorld->getIntConfig(CONFIG_CUSTOM_XP_RATE_DEFAULT);

				player->SetCustomXPRate(defaultRate);
			}
		}
	}
};

class custom_rate_commands : public CommandScript
{
private:

public:
	custom_rate_commands() : CommandScript("custom_rate_commands")
	{
	}

	std::vector<ChatCommand> GetCommands() const override
	{
		static std::vector<ChatCommand> xpCommandTable =
		{
			{ "set", SEC_PLAYER, false, &HandleSetExpRateCommand, "" },
			{ "get", SEC_PLAYER, false, &HandleGetExpRateCommand, "" },
			{ "active", SEC_ADMINISTRATOR, false, &HandleExpRateToggleCommand, "" }
		};

		static std::vector<ChatCommand> rateTypeCommandTable =
		{
			{ "xp", SEC_PLAYER, false, nullptr, "", xpCommandTable },
		};

		static std::vector<ChatCommand> commandTable =
		{
			{ "rate", SEC_PLAYER, false, nullptr, "", rateTypeCommandTable }
		};

		return commandTable;
	}

	static bool HandleExpRateToggleCommand(ChatHandler *handler, char const *args)
	{
		// Attempt to retrieve the player who executed the command.
		Player *me = handler->GetSession() ? handler->GetSession()->GetPlayer() : NULL;
		if (!me)
			return false;

		// Is the command accessible by our security level?
		if (me->GetSession()->GetSecurity() < SEC_ADMINISTRATOR)
		{
			handler->SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
			handler->SetSentErrorMessage(true);
			return false;
		}


		if (!*args)
		{
			handler->PSendSysMessage("Usage: .rate xp active [0/1]");
			handler->SetSentErrorMessage(true);
			return false;
		}

		Tokenizer tokens(args, ' ');

		if (!*args || !tokens.size() || tokens.size() != 1)
		{
			handler->PSendSysMessage("Usage: .rate xp active [0/1]");
			handler->SetSentErrorMessage(true);
			return false;
		}

		// Get input from user.
		Tokenizer::const_iterator i = tokens.begin();
		uint32 rateState = atoi(*(i++));

		bool toggle = (rateState > 0) ? true : false;

		// No need to update this.
		if (toggle == sWorld->getBoolConfig(CONFIG_CUSTOM_XP_RATE))
		{
			handler->PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: Custom rates for experience are already %s.", toggle ? "on" : "off");
			handler->SetSentErrorMessage(true);
			return false;
		}

		// Update bool config.
		handler->PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: Custom rates for experience are now %s.", toggle ? "on" : "off");
		sWorld->setBoolConfig(CONFIG_CUSTOM_XP_RATE, toggle);
		return true;
	}

	static bool HandleGetExpRateCommand(ChatHandler *handler, const char *args)
	{
		// take a pointer to the player who uses the command
		Player *me = handler->GetSession() ? handler->GetSession()->GetPlayer() : NULL;
		if (!me)
			return false;

		// Are we even allowed to change it?
		if (!sWorld->getBoolConfig(CONFIG_CUSTOM_XP_RATE))
		{
			handler->SendSysMessage("|CFF7BBEF7[Custom Rates]|r: Viewing your XP rate is disabled at this time.");
			return true;
		}

		handler->PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: Your current XP rate is %u.", me->GetCustomXPRate());
		return true;
	}

	static bool HandleSetExpRateCommand(ChatHandler *handler, const char *args)
	{
		// take a pointer to the player who uses the command
		Player *me = handler->GetSession() ? handler->GetSession()->GetPlayer() : NULL;
		if (!me)
			return false;

		// Are we even allowed to change it?
		if (!sWorld->getBoolConfig(CONFIG_CUSTOM_XP_RATE))
		{
			handler->SendSysMessage("|CFF7BBEF7[Custom Rates]|r: Changing your experience rate is disabled at this time.");
			return true;
		}

		// already at max level, no point in using the command at all
		if (me->getLevel() == sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL))
		{
			handler->SendSysMessage("|CFF7BBEF7[Custom Rates]|r: You are already at maximum level.");
			return true;
		}

		// first, check if I can use the command
		if (me->GetSession()->GetSecurity() < (int)sWorld->getIntConfig(CONFIG_CUSTOM_XP_RATE_SECURITY))
		{
			handler->SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
			handler->SetSentErrorMessage(true);
			return false;
		}

		// take a pointer to player's selection
		Player *target = handler->getSelectedPlayer();
		if (!target || !target->GetSession())
		{
			handler->SendSysMessage(LANG_NO_CHAR_SELECTED);
			handler->SetSentErrorMessage(true);
			return false;
		}

		// extract value
		int rate = atoi((char *)args);
		int maxRate = sWorld->getIntConfig(CONFIG_CUSTOM_XP_RATE_MAX);
		if (rate < 0 || rate > maxRate)
		{
			handler->PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: Invalid rate specified, must be in interval [0,%i].", maxRate);
			handler->SetSentErrorMessage(true);
			return false;
		}

		// take a pointer to the player we need to set xp rate to
		// can be either player itself, or his selection, if
		// selection security is lower than his security
		Player *player = NULL;
		if (target == me)
			player = me;
		else
		{
			if (me->GetSession()->GetSecurity() > target->GetSession()->GetSecurity())
				player = target;
			else
			{
				handler->SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
				handler->SetSentErrorMessage(true);
				return false;
			}
		}

		// set player custom XP rate and save it in DB for later use
		uint32 uRate = static_cast<uint32>(rate);
		player->SetCustomXPRate(uRate);
		int32 rateFromDB = CustomRates::GetXpRateFromDB(player);
		if (rateFromDB == -1)
			CustomRates::SaveXpRateToDB(player, uRate, false);
		else
			CustomRates::SaveXpRateToDB(player, uRate, true);

		// show a message indicating custom XP rate change
		if (player == me)
			handler->PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: You have set your XP rate to %u.", uRate);
		else
		{
			handler->PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: You have set %s's XP rate to %u.", handler->GetNameLink(player).c_str(), uRate);
			ChatHandler(player->GetSession()).PSendSysMessage("|CFF7BBEF7[Custom Rates]|r: %s has set your XP rate to %u.", handler->GetNameLink().c_str(), uRate);
		}

		return true;
	}
};

void AddSC_Custom_Rates()
{
	new add_del_rates();
	new custom_rate_commands();
}