// Nonolith Connect
// https://github.com/nonolith/connect
// JSON REST endpoint handlers
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <string>
#include <boost/foreach.hpp>

#include "websocketpp.hpp"
#include "url.hpp"
#include "json.hpp"

void respondJSON(websocketpp::session_ptr client, JSONNode &n, int status){
	string jc = (string) n.write_formatted();
	client->start_http(status, jc);
}


void respondError(websocketpp::session_ptr client, std::exception& e){
	JSONNode j;
	j.push_back(JSONNode("error", e.what()));
	std::cerr << "Exception while processing request: " << e.what() <<std::endl;
	respondJSON(client, j, 402);
}

void deviceListRequest(websocketpp::session_ptr client){
	JSONNode n = jsonDevicesArray(true);
	respondJSON(client, n);
}

bool deviceRequest(UrlPath path, websocketpp::session_ptr client){
	device_ptr device = getDeviceById(path.get());
	if (device)
		return device->handleREST(path.sub(), client);
	else
		return false;
}

void handleJSONRequest(UrlPath path, websocketpp::session_ptr client){
	client->set_header("Access-Control-Allow-Origin", client->get_client_header("Origin"));
	if (path.leaf()){
		client->start_http(404, "No API version selected.");
		return;
	}else if (path.matches("v1")){
		UrlPath path1 = path.sub();
		
		if(path1.leaf()){
			JSONNode n(JSON_NODE);
			n.push_back(JSONNode("server", "Nonolith Connect"));
			n.push_back(JSONNode("version", server_version));
			n.push_back(JSONNode("gitVersion", server_git_version));
			respondJSON(client, n);
			return;
		}else if (path1.matches("devices")){
			UrlPath path2 = path1.sub();
			
			if (path2.leaf()){
				return deviceListRequest(client);
			}else{
				if (deviceRequest(path2, client)) return;
			}
		}
	}else{
		client->start_http(404, "API version not supported");
		return;
	}
	
	client->start_http(404, "REST object not found");
}
