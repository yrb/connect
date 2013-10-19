// Nonolith Connect
// https://github.com/nonolith/connect
// USB event handling
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#include <iostream>
#include <vector>
#include <map>
#include <boost/thread.hpp>

#include "dataserver.hpp"
#include "cee/cee.hpp"
#include "bootloader/bootloader.hpp"
#include <libusb/libusb.h>

using namespace std;

map <libusb_device *, device_ptr> active_libusb_devices;

boost::thread* usb_thread;

#define NONOLITH_VID 0x59e3
#define CEE_PID 0xCEE1
#define BOOTLOADER_PID 0xBBBB

libusb_hotplug_callback_handle hotplug_handle;
extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
	libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data);


void usb_init(){
	int r = libusb_init(NULL);
	if (r < 0){
		cerr << "Could not init libusb" << endl;
		abort();
	}

	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		cerr << "Using libusb hotplug" << endl;
		libusb_hotplug_register_callback(NULL, 
			(libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
			(libusb_hotplug_flag) 0,
			LIBUSB_HOTPLUG_MATCH_ANY,
			LIBUSB_HOTPLUG_MATCH_ANY,
			LIBUSB_HOTPLUG_MATCH_ANY,
			hotplug_callback_usbthread,
			NULL,
			&hotplug_handle
		);
	} else {
		cerr << "Libusb hotplug not supported. Only devices already attached will be used." << endl;
	}

	usb_thread = new boost::thread(usb_thread_main);
}

// Dedicated thread for handling soft-real-time USB events
void usb_thread_main(){
	while(1) libusb_handle_events(NULL);
}

void usb_fini(){
	libusb_hotplug_deregister_callback(NULL, hotplug_handle);
	usb_thread->join();

	devices.empty();
	
	libusb_exit(NULL);
}

void deviceAdded(libusb_device *dev){
	libusb_device_descriptor desc;
	int r = libusb_get_device_descriptor(dev, &desc);
	if (r<0){
		cerr << "Error in get_device_descriptor" << endl;
		libusb_unref_device(dev);
		return;
	}
	
	device_ptr newDevice;
	
	try{
		if (desc.idVendor == NONOLITH_VID || desc.idVendor == 0x9999){
			if (desc.idProduct == CEE_PID){
				newDevice = device_ptr(new CEE_device(dev, desc));
			}else if (desc.idProduct == BOOTLOADER_PID || desc.idProduct == 0xb003){
				newDevice = device_ptr(new Bootloader_device(dev, desc));
			}
		}
	}catch(std::exception e){
		cerr << "Error initializing new device. Ignoring." << std::endl;
	}
	
	if (newDevice){
		devices.insert(newDevice);
		active_libusb_devices.insert(pair<libusb_device *, device_ptr>(dev, newDevice));
		device_list_changed.notify();
	}
	
	libusb_unref_device(dev);
}

void deviceRemoved(libusb_device *dev){
	map <libusb_device *, device_ptr>::iterator it = active_libusb_devices.find(dev);
	if (it == active_libusb_devices.end()) return;
	cerr << "Device removed" <<std::endl;
	device_ptr ds_dev = it->second;
	devices.erase(ds_dev);
	active_libusb_devices.erase(it);
	device_list_changed.notify();
	ds_dev->onDisconnect();
}

extern "C" int LIBUSB_CALL hotplug_callback_usbthread(
	libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data) {
	if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
		libusb_ref_device(device);
		io.post(boost::bind(deviceAdded, device));
	} else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
		io.post(boost::bind(deviceRemoved, device));
	}
	return 0;
}

void usb_scan_devices(){
	libusb_device **devs;
	
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0){
		cerr << "Error in get_device_list" << endl;
	}

	for (ssize_t i=0; i<cnt; i++){
		libusb_ref_device(devs[i]);
		deviceAdded(devs[i]);
	}

	libusb_free_device_list(devs, 1);
}

bool USB_device::processMessage(ClientConn& client, string& cmd, JSONNode& n){
	if (cmd == "controlTransfer"){
		unsigned id = jsonIntProp(n, "id", 0);
		uint8_t bmRequestType = jsonIntProp(n, "bmRequestType", 0xC0);
		uint8_t bRequest = jsonIntProp(n, "bRequest");
		uint16_t wValue = jsonIntProp(n, "wValue", 0);
		uint16_t wIndex = jsonIntProp(n, "wIndex", 0);
	
		bool isIn = bmRequestType & 0x80;
		
		JSONNode reply(JSON_NODE);
		reply.push_back(JSONNode("_action", "return"));
		reply.push_back(JSONNode("id", id));
		
		int ret = -1000;
		
		if (isIn){
			uint16_t wLength = jsonIntProp(n, "wLength", 64);
			if (wLength > 64) wLength = 64;
			if (wLength < 0) wLength = 0;
		
			uint8_t data[wLength];
			ret = controlTransfer(bmRequestType, bRequest, wValue, wIndex, data, wLength);
			
			if (ret >= 0){
				JSONNode data_arr(JSON_ARRAY);
				for (int i=0; i<ret && i<wLength; i++){
					data_arr.push_back(JSONNode("", data[i]));
				}
				data_arr.set_name("data");
				reply.push_back(data_arr);
			}
		}else{
			string datastr;
			JSONNode data = n.at("data");
			if (data.type() == JSON_ARRAY){
				for(JSONNode::iterator i=data.begin(); i!=data.end(); i++){
					datastr.push_back(i->as_int());
				}
			}else{
				datastr = data.as_string();
			}
			ret = controlTransfer(bmRequestType, bRequest, wValue, wIndex, (uint8_t *)datastr.data(), datastr.size());
		}
		
		reply.push_back(JSONNode("status", ret));
	
		client.sendJSON(reply);
	}else if(cmd == "enterBootloader"){
		std::cout << "enterBootloader: ";
		int r = controlTransfer(0x40|0x80, 0xBB, 0, 0, NULL, 100);
		std::cout << "return " <<  r << std::endl;
	}else{
		return false;
	}
	return true;
}


