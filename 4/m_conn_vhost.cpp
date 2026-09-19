/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <connect vhost="vhost.here">
/// $ModDepends: core 4
/// $ModDesc: Sets a connect block configured vhost on users when they connect
// The use of '$ident' will be replaced with the user's ident
// The use of '$account' will be replaced with the user's account name


#include "inspircd.h"
#include "modules/account.h"

class ModuleVhostOnConnect final
	: public Module
{
private:
	Account::API accountapi;
	CharState hostmap;

	std::string GetAccount(LocalUser* user)
	{
		const std::string* account = accountapi ? accountapi->GetAccountName(user) : nullptr;
		return account ? *account : "";
	}

public:
	ModuleVhostOnConnect()
		: Module(VF_VENDOR, "Sets a connect block configured vhost on users when they connect.")
		, accountapi(this)
	{
	}

	void Prioritize() override
	{
		// Let's go after conn_umodes in case +x also gets set
		ServerInstance->Modules.SetPriority(this, I_OnUserConnect, PRIORITY_AFTER, "conn_umodes");
	}

	void ReadConfig(ConfigStatus& status) override
	{
		// From m_sethost: use the same configured host character map if it exists
		const std::string hmap = ServerInstance->Config->ConfValue("hostname")->getString(
			"charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789");

		hostmap.reset();
		for (const auto chr : hmap)
			hostmap.set(static_cast<unsigned char>(chr));
	}

	void OnUserConnect(LocalUser* user) override
	{
		const auto& tag = user->GetClass()->config;
		std::string vhost = tag->getString("vhost");
		std::string replace;
		size_t pos;

		if (vhost.empty())
			return;

		std::string ident = user->GetRealUser();
		if (ident[0] == '~')
			ident.erase(0, 1);

		replace = "$ident";
		while ((pos = irc::find(vhost, replace)) != std::string::npos)
			vhost.replace(pos, replace.length(), ident);

		std::string account = GetAccount(user);
		if (account.empty())
			account = "unidentified";

		replace = "$account";
		while ((pos = irc::find(vhost, replace)) != std::string::npos)
			vhost.replace(pos, replace.length(), account);

		if (vhost.length() > ServerInstance->Config->Limits.MaxHost)
		{
			ServerInstance->Logs.Warning(MODNAME,
				"vhost in connect block {} is too long",
				user->GetClass()->GetName());
			return;
		}

		// From m_sethost: validate the characters
		for (const auto chr : vhost)
		{
			if (!hostmap.test(static_cast<unsigned char>(chr)))
			{
				ServerInstance->Logs.Warning(MODNAME,
					"vhost in connect block {} has invalid characters",
					user->GetClass()->GetName());
				return;
			}
		}

		user->ChangeDisplayedHost(vhost);
	}
};

MODULE_INIT(ModuleVhostOnConnect)
