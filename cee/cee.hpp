// Nonolith Connect
// https://github.com/nonolith/connect
// USB Data streaming for CEE
// Released under the terms of the GNU GPLv3+
// (C) 2012 Nonolith Labs, LLC
// Authors:
//   Kevin Mehall <km@kevinmehall.net>

#pragma once

#include "../dataserver.hpp"
#include "../streaming_device/streaming_device.hpp"
#include "../usb_device.hpp"
#include <boost/thread/mutex.hpp>

enum CEE_chanmode{
	DISABLED = 0,
	SVMI = 1,
	SIMV = 2,
};

inline int16_t signextend12(uint16_t v){
	return (v>((1<<11)-1))?(v - (1<<12)):v;
}

struct IN_sample{
	uint8_t avl, ail, aih_avh, bvl, bil, bih_bvh;

	int16_t av(){return signextend12((aih_avh&0x0f)<<8) | avl;}
	int16_t bv(){return signextend12((bih_bvh&0x0f)<<8) | bvl;}
	int16_t ai(){return signextend12((aih_avh&0xf0)<<4) | ail;}
	int16_t bi(){return signextend12((bih_bvh&0xf0)<<4) | bil;}
} __attribute__((packed));

#define IN_SAMPLES_PER_PACKET 10
#define FLAG_PACKET_DROPPED (1<<0)

typedef struct IN_packet{
	uint8_t mode_a;
	uint8_t mode_b;
	uint8_t flags;
	uint8_t mode_seq;
	IN_sample data[10];	
} __attribute__((packed)) IN_packet;

#define OUT_SAMPLES_PER_PACKET 10
struct OUT_sample{
	uint8_t al, bl, bh_ah;
	void pack(uint16_t a, uint16_t b){
		al = a & 0xff;
		bl = b & 0xff;
		bh_ah = ((b>>4)&0xf0) |	(a>>8);
	}
} __attribute__((packed));

typedef struct OUT_packet{
	uint8_t mode_a;
	uint8_t mode_b;
	OUT_sample data[10];
} __attribute__((packed)) OUT_packet;

class OutputPacketSource;


#define EEPROM_VALID_MAGIC 0x90e26cee

struct EEPROM_cal{
	uint32_t magic;
	int8_t offset_a_v, offset_a_i, offset_b_v, offset_b_i;
	int16_t dac200_a, dac200_b, dac400_a, dac400_b;
	uint32_t current_gain_a, current_gain_b;
	uint8_t flags; // bit 0: USB powered
} __attribute__((packed));

#define EEPROM_FLAG_USB_POWER (1<<0)

typedef struct CEE_version_descriptor{
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t flags;
	uint8_t per_ns;
	uint8_t min_per;
} __attribute__((packed)) CEE_version_descriptor;

#define N_TRANSFERS 64

class CEE_device: public StreamingDevice, USB_device{
	public: 
	CEE_device(libusb_device *dev, libusb_device_descriptor &desc);
	virtual ~CEE_device();
	
	virtual void configure(int mode, double sampleTime, unsigned samples, bool continuous, bool raw);

	virtual const string model(){return "com.nonolithlabs.cee";}
	virtual const string hwVersion(){return _hwversion;}
	virtual const string fwVersion(){
		string r = _fwversion;
		if (_gitversion.size()) r += "/" + _gitversion;
		return r;
	}
	virtual const string serialno(){return serial;}
	
	virtual bool processMessage(ClientConn& session, string& cmd, JSONNode& n);
	virtual bool handleREST(UrlPath path, websocketpp::session_ptr client);
	virtual void handleRESTGPIOCallback(websocketpp::session_ptr client, string postdata);
	
	JSONNode gpio(bool set, uint8_t dir=0, uint8_t out=0);
	virtual void setOutput(Channel* channel, OutputSource* source);
	virtual void setInternalGain(Channel* channel, Stream* stream, int gain);

	libusb_transfer* in_transfers[N_TRANSFERS];
	libusb_transfer* out_transfers[N_TRANSFERS];

	Channel channel_a;
	Channel channel_b;

	Stream channel_a_v;
	Stream channel_a_i;
	Stream channel_b_v;
	Stream channel_b_i;

	boost::mutex outputMutex;
	boost::mutex transfersMutex;
	void fillOutTransfer(unsigned char*);
	void handleInTransfer(unsigned char*);
	
	virtual void setCurrentLimit(unsigned limit);

	/// count of IN and OUT packets, owned by USB thread
	unsigned incount, outcount;

	bool firstPacket;
	
	int ntransfers, packets_per_transfer;

	protected:
	string _hwversion, _fwversion, _gitversion;
	virtual void on_reset_capture();
	virtual void on_start_capture();
	virtual void on_pause_capture();
	uint16_t encode_out(CEE_chanmode mode, float val, uint32_t igain);
	void checkOutputEffective(Channel& channel);
	
	EEPROM_cal cal;

	int min_per;
	int xmega_per;
	void readCalibration();
};
