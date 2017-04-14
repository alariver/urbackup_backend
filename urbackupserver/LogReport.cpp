#include "LogReport.h"
#include "../Interface/Server.h"
#include "../Interface/Mutex.h"
#include "../Interface/Database.h"
#include "../stringtools.h"
#include "database.h"
#include "../luaplugin/ILuaInterpreter.h"
#include "Mailer.h"

extern ILuaInterpreter* lua_interpreter;

namespace
{
	IMutex* mutex;
	std::string script;

	class MailBridge : public ILuaInterpreter::IEMailFunction
	{
	public:
		// Inherited via IEMailFunction
		virtual bool mail(const std::string & send_to, const std::string & subject, const std::string & message)
		{
			return Mailer::sendMail(send_to, subject, message);
		}
	};

	ILuaInterpreter::SInterpreterFunctions funcs;

	IMutex* global_mutex;
	std::string global_data;
}

std::string load_report_script()
{
	IScopedLock lock(mutex);

	if (!script.empty())
	{
		return script;
	}

	if (FileExists("urbackupserver/report.lua"))
	{
		script = getFile("urbackupserver/report.lua");
		if (!script.empty())
		{
			return script;
		}
	}

	IDatabase* db = Server->getDatabase(Server->getThreadID(), URBACKUPDB_SERVER);

	db_results res = db->Read("SELECT tvalue FROM misc WHERE tkey='report_script'");

	if (!res.empty())
	{
		return res[0]["tvalue"];
	}

	return std::string();
}

std::string get_report_script()
{
	std::string script = load_report_script();
	if (script.empty())
		return script;

	std::string ret = lua_interpreter->compileScript(script);
	return ret;
}

void init_log_report()
{
	mutex = Server->createMutex();
	funcs.mail_func = new MailBridge;
	global_mutex = Server->createMutex();
}

void reload_report_script()
{
	IScopedLock lock(mutex);
	script.clear();
}

bool run_report_script(int incremental, bool resumed, int image,
	int infos, int warnings, int errors, bool success, const std::string& report_mail, 
	const std::string & data, const std::string& clientname)
{
	std::string script;
	{
		IScopedLock lock(mutex);
		script = get_report_script();
	}

	if (script.empty())
		return false;

	ILuaInterpreter::Param param_raw;
	ILuaInterpreter::Param::params_map& param = *param_raw.u.params;

	param["incremental"] = incremental;
	param["resumed"] = resumed;
	param["image"] = image;
	param["infos"] = infos;
	param["warnings"] = warnings;
	param["warnings"] = errors;
	param["report_mail"] = report_mail;
	param["clientname"] = clientname;
	ILuaInterpreter::Param::params_map& pdata = *param["data"].u.params;
	
	std::vector<std::string> msgs;
	TokenizeMail(data, msgs, "\n");

	for (size_t j = 0; j<msgs.size(); ++j)
	{
		ILuaInterpreter::Param::params_map& obj = *pdata[static_cast<int>(j + 1)].u.params;
		std::string ll;
		if (!msgs[j].empty()) ll = msgs[j][0];
		int li = watoi(ll);
		msgs[j].erase(0, 2);
		std::string tt = getuntil("-", msgs[j]);
		std::string m = getafter("-", msgs[j]);
		obj["time"] = watoi64(tt);
		obj["msg"] = m;
		obj["ll"] = li;
	}

	int64 ret2;
	std::string state;
	int64 ret;
	{
		IScopedLock lock(global_mutex);
		ret = lua_interpreter->runScript(script, param_raw, ret2, state, global_data, funcs);
	}

	if (ret<0)
	{
		return false;
	}

	return true;
}


