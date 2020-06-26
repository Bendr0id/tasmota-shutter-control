/* Tasmota-Shutter-Control
 * Copyright 2020-     BenDr0id    <https://github.com/BenDr0id>, <ben@graef.in>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include <arpa/inet.h>
#include <CLI/CLI11.hpp>
#include <cpp-httplib/httplib.h>
#include <rapidjson/document.h>

constexpr char NAME[] = "Tasmota-Shutter-Control";
constexpr char VERSION[] = "1.0";

constexpr char OP_UP[] = "UP";
constexpr char OP_DOWN[] = "DOWN";
constexpr char OP_SELECT[] = "SELECT";
constexpr char OP_STOP[] = "STOP";

struct Params
{
  std::string ip;
  std::string operation;
  int shutter = 0;
};

const static std::map<std::string, int> OperationPortMap =
{
  {OP_UP, 1},
  {OP_DOWN, 2},
  {OP_SELECT, 3},
  {OP_STOP, 4},
};

const static std::map<int, int> ShutterPortMap =
{
  {1, 1},
  {2, 2},
  {3, 3},
  {4, 4},
};

class IPValidator : public CLI::Validator
{
  public:
  IPValidator()
  {
    name_ = "IP";
    func_ = [](const std::string &str)
    {
      unsigned char buf[sizeof(struct in_addr)];
      if (inet_pton(AF_INET, str.c_str(), buf) <= 0)
      {
        return "Invalid IP Addresse: " + str;
      }

      return std::string();
    };
  }
};
const static IPValidator IPValidator;

bool isLedOn(std::map<int, bool>& ledStates, int ledIdx)
{
  return ledStates.find(ledIdx) != ledStates.end() && ledStates[ledIdx];
}

bool areAllLedsOn(const std::map<int, bool>& ledStates)
{
  return std::all_of(std::next(ledStates.begin()), ledStates.end(),
    [](typename std::map<int, bool>::const_reference t){ return t.second; });
}

std::shared_ptr<httplib::Response> performRequest(const std::shared_ptr<Params>& params, const std::string& path, const std::string& body)
{
    auto cli = std::make_shared<httplib::Client>(params->ip);

    httplib::Request req;
    req.method = "GET";
    req.path = path;
    req.set_header("Host", "");
    req.set_header("Accept", "*//*");
    req.set_header("User-Agent", NAME);
    req.set_header("Accept", "application/json");
    req.set_header("Content-Type", "application/json");

    if (!body.empty())
    {
      req.body = body;
    }

    auto res = std::make_shared<httplib::Response>();

    return cli->send(req, *res) ? res : nullptr;
}

std::map<int, bool> getLedStates(const std::shared_ptr<Params>& params)
{
  std::map<int, bool> ledStates;

  auto res = performRequest(params, "/cm?cmnd=Status%208", "");
  if (!res || res->status != 200)
  {
    std::cerr << "Failed to getLedStates. HttpResult: " << res ? res->status : -1;
  }
  else
  {
    rapidjson::Document document;
    if (!document.Parse(res->body.c_str()).HasParseError())
    {
      if (document.HasMember("StatusSNS"))
      {
        const auto& status = document["StatusSNS"];
        for (const auto& element : status.GetObject())
        {
          const auto name = std::regex_replace(element.name.GetString(), std::regex("\\Switch"), "");
          if (!name.empty() && name != "Time")
          {
            ledStates[std::stoi(name)] = element.value.GetString() == std::string("ON");
          }
        }
      }
    }
    else
    {
      std::cerr << "Failed to getLedStates. Failed to parse json: " << res->body;
    }
  }

  return ledStates;
}

bool pushButton(const std::shared_ptr<Params>& params, int port)
{
  std::stringstream requestUrl;
  requestUrl << "/cm?cmnd=Power" <<  port << "%20ON";

  auto res = performRequest(params, requestUrl.str(), "");
  if (!res || res->status != 200)
  {
    std::cerr << "Failed to pushButton. HttpResult: " << res ? res->status : -1;
  }

  return (res && res->status == 200);
}

bool selectShutter(const std::shared_ptr<Params>& params)
{
  bool res = true;
  int retries = 0;

  auto ledStates = getLedStates(params);
  while (!isLedOn(ledStates, ShutterPortMap[params->shutter]) || areAllLedsOn(ledStates))
  {
    if (!pushButton(params, OperationPortMap[OP_SELECT]) || retries > (ShutterPortMap.size() * 2))
    {
      std::cerr << "Failed to selectShutter: " << params->shutter;
      res = false;
      break;
    }

    retries++;
  }

  return res;
}

int main(int argc, char** argv)
{
  std::shared_ptr<Params> params = std::make_shared<Params>();

  CLI::App app(std::string(NAME) + " " + std::string(VERSION));

  app.add_option("-i, --ip", params->ip, "The IP of the Tasmota device")->required()->type_name("ADDRESS")->check(IPValidator);
  app.add_option("-o, --operation", params->operation, "The Operation to execute")->required()->type_name("OP")->check(CLI::IsMember({OP_UP, OP_DOWN, OP_SELECT, OP_STOP}));
  app.add_option("-s, --shutter", params->shutter, "The number of the shutter to control")->required()->type_name("NUM")->check(CLI::Range(1,4));

  CLI11_PARSE(app, argc, argv);

  std::cout << NAME << " " << VERSION << "\n";

  bool ret = selectShutter(params);
  if (ret)
  {
    ret = pushButton(params, OperationPortMap[params->operation]);
  }

  return ret ? 0 : 1;
}

