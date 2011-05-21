/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011  Martin Raiber
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef CLIENT_ONLY

#include "action_header.h"

ACTION_IMPL(login)
{
	Helper helper(tid, &GET, &PARAMS);
	IDatabase *db=helper.getDatabase();

	bool has_session=false;
	std::wstring ses;
	if(!GET[L"ses"].empty())
	{
		ses=GET[L"ses"];
		has_session=true;
	}

	JSON::Object ret;

	if(pychart_fak==NULL)
	{
		ret.set("use_googlechart", true);
	}

	std::wstring username=GET[L"username"];
	if(!username.empty())
	{
		/*if(has_session==false)
		{
			ses=helper.generateSession(L"anonymous");
			GET[L"ses"]=ses;
			ret.set("session", JSON::Value(ses));
			helper.update(tid, &GET, &PARAMS);
		}*/
		SUser *session=helper.getSession();
		if(session!=NULL)
		{
			IQuery *q=db->Prepare("SELECT id, name, password_md5 FROM si_users WHERE name=?");
			q->Bind(username);
			db_results res=q->Read();
			if(!res.empty())
			{
				std::wstring password_md5=res[0][L"password_md5"];
				std::string ui_password=wnarrow(GET[L"password"]);
				std::string r_password=Server->GenerateHexMD5(Server->ConvertToUTF8(session->mStr[L"rnd"]+password_md5));
				if(r_password!=ui_password)
				{
					ret.set("error", JSON::Value(2));
				}
				else
				{
					ret.set("success", JSON::Value(true));
					session->mStr[L"login"]=L"ok";
					session->id=watoi(res[0][L"id"]);

					ret.set("status", helper.getRights("status") );
					ret.set("graph", helper.getRights("piegraph"));
					ret.set("progress", helper.getRights("progress") );
					ret.set("browse_backups", helper.getRights("browse_backups") );
					ret.set("settings", helper.getRights("settings") );
					ret.set("logs", helper.getRights("logs") );
				}
			}
			else
			{
				ret.set("error", JSON::Value(0));
			}
		}
		else
		{
			ret.set("error", JSON::Value(1));
		}
	}
	else
	{
		ret.set("lang", helper.getLanguage());
		db_results res=db->Read("SELECT count(*) AS c FROM si_users");
		if(watoi(res[0][L"c"])>0)
		{
			ret.set("success", JSON::Value(false) );
		}
		else
		{
			ret.set("success", JSON::Value(true) );
			if(has_session==false)
			{
				ses=helper.generateSession(L"anonymous");
				GET[L"ses"]=ses;
				ret.set("session", JSON::Value(ses));
				helper.update(tid, &GET, &PARAMS);
			}
			SUser *session=helper.getSession();
			if(session!=NULL)
			{
				session->mStr[L"login"]=L"ok";
				session->id=0;
			}
			else
			{
				ret.set("error", JSON::Value(1));
			}
		}
	}

	helper.Write(ret.get(false));
}

#endif //CLIENT_ONLY