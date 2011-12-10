#pragma once
#ifndef INCLUDE_FROM_DATASERVER_HPP
#error "device.hpp should not be included directly"
#endif

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

struct Channel;
struct Stream;
struct OutputSource;

enum CaptureState{
	CAPTURE_INACTIVE,
	CAPTURE_READY,
	CAPTURE_ACTIVE,
	CAPTURE_PAUSED,
	CAPTURE_DONE,
};

string captureStateToString(CaptureState s);

class Device: public boost::enable_shared_from_this<Device> {
	public: 
		Device(): captureState(CAPTURE_INACTIVE), captureLength(0) {}
		virtual ~Device(){};
		
		/// Allocate resources to capture the specified number of seconds of data
		void prepare_capture(float seconds);
		void start_capture();
		void pause_capture();

		virtual const string getId(){return model()+"~"+serialno();}
		virtual const string serialno(){return "0";}
		virtual const string model() = 0;
		virtual const string hwVersion(){return "unknown";}
		virtual const string fwVersion(){return "unknown";}

		virtual void setOutput(Channel* channel, OutputSource* source);

		Channel* channelById(const std::string&);

		Event dataReceived;

		Event captureStateChanged;
		CaptureState captureState;
		float captureLength;

		std::vector<Channel*> channels;

	protected:
		virtual void on_prepare_capture() = 0;
		virtual void on_start_capture() = 0;
		virtual void on_pause_capture() = 0;
		void done_capture();
};

typedef boost::shared_ptr<Device> device_ptr;

struct Channel{
	Channel(const string _id, const string _dn):
		id(_id), displayName(_dn), source(0){}
	const string id;
	const string displayName;

	Stream* streamById(const std::string&);
	
	std::vector<Stream*> streams;
	OutputSource *source;
};

struct Stream{
	Stream(const string _id, const string _dn, const string _units, float _min, float _max,
				const string startState, float _sampleTime, unsigned _outputMode=0):
		id(_id),
		displayName(_dn),
		units(_units),
		min(_min),
		max(_max),
		state(startState),
		data(0),
		buffer_size(0),
		buffer_fill_point(0),
		sampleTime(_sampleTime),
		outputMode(_outputMode){};

	~Stream(){
		if (data){
			free(data);
		}
	}

	const string id;
	const string displayName;
	const string units;

	float min, max;

	string state;

	/// Raw data buffer
	float* data;

	/// Allocated elements of *data
	unsigned buffer_size;

	/// data[i] for i<buffer_fill_point is valid
	unsigned buffer_fill_point;

	/// Allocate space for /size/ samples
	void allocate(unsigned size);

	/// Store a sample and increment buffer_fill_point
	/// Note: when you are done putting samples, call data_received.notify()
	/// This is not called automatically, because it only needs to be called once
	/// if multiple samples are put at the same time.
	void put(float p);
	
	float sampleTime;

	/// mode for output that "sources" this stream's variable
	/// 0 if outputting this variable is not supported.
	unsigned outputMode;
};

struct OutputSource{
	virtual string displayName() = 0;
	virtual float nextValue(float time) = 0;

	const unsigned mode;

	protected:
		OutputSource(unsigned m): mode(m){};
};

struct ConstantOutputSource: public OutputSource{
	ConstantOutputSource(unsigned m, float val): OutputSource(m), value(val){}
	virtual string displayName(){return "Constant";};
	virtual float nextValue(float time){
		return value;
	}
	float value;
};

device_ptr getDeviceById(string id);
Stream* findStream(const string& deviceId, const string& channelId, const string& streamId);
