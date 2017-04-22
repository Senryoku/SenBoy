#pragma once

#include <queue>

#include <SFML/Audio.hpp>

#include "gb_apu/Gb_Apu.h"
#include "gb_apu/Multi_Buffer.h"

class GBAudioStream : public sf::SoundStream
{
public:
	const size_t chunk_size = 3000;
	const size_t buffer_size = chunk_size * 20;
	
	GBAudioStream(unsigned int sampleRate = 44100)
	{
		initialize(2, sampleRate);
		_buffer = new blip_sample_t[buffer_size];
		_buff_max = buffer_size;
	}
	
	~GBAudioStream()
	{
		delete[] _buffer;
	}

	/// Avoid a memcpy by exposing _buffer
	/// Don't use both versions of add_samples at the same time!
	blip_sample_t* add_samples(long int count)
	{
		assert(count < buffer_size);
		blip_sample_t* r = _buffer + _buff_end;
		_buff_end += count;
		
		if(_buff_end >= buffer_size) // Too many samples at once...
			r = _buffer + buffer_size - 1 - count;
		
		if(_buff_end > buffer_size - chunk_size * 2)
		{
			_buff_max = std::min(_buff_end, buffer_size - 1);
			_buff_end = 0;
		}
		return r;
	}
	
	/// Don't use both versions of add_samples at the same time!
	void add_samples(blip_sample_t* samples, long int count)
	{
		long int empty_space = buffer_size - _buff_end;
		if(count > empty_space)
		{
			std::memcpy(_buffer + _buff_end, samples, empty_space * sizeof(blip_sample_t));
			_buff_end = 0;
			samples += empty_space;
			count -= empty_space;
		}
		
		std::memcpy(_buffer + _buff_end, samples, count * sizeof(blip_sample_t));
		_buff_end = (_buff_end + count) % buffer_size;
	}
	
	void reset()
	{
		stop();
		std::memset(_buffer, 0, buffer_size * sizeof(blip_sample_t));
		_offset = 0;
		_buff_end = 0;
		_buff_max = buffer_size;
	}
	
	const blip_sample_t* get_buffer() const { return _buffer; }
	blip_sample_t*       get_buffer()       { return _buffer; }
	
private:
	virtual bool onGetData(Chunk& data)
    {
		if(_buff_end > _offset && _buff_end - _offset < chunk_size)
			return false;
		
        data.samples = _buffer + _offset;
		if(_buff_max - _offset < chunk_size)
		{
			data.sampleCount = _buff_max - _offset;
			_offset = 0;
		} else {
			data.sampleCount = chunk_size;
			_offset += chunk_size;
		}
		
        return true;
    }
	
    virtual void onSeek(sf::Time timeOffset)
    {
        _offset = static_cast<size_t>(timeOffset.asSeconds() * getSampleRate() * getChannelCount()) % buffer_size;
    }
	
	blip_sample_t*				_buffer;
	
	size_t						_offset = 0;
	size_t						_buff_end = 0;
	size_t						_buff_max = 0;
};
