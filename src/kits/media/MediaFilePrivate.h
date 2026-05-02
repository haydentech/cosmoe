#ifndef COSMOE_MEDIA_FILE_PRIVATE_H
#define COSMOE_MEDIA_FILE_PRIVATE_H


#include <MediaDefs.h>
#include <MediaFormats.h>

#include <vector>


class BDataIO;


namespace BPrivate { namespace media {

struct DecodedTrackInfo {
	media_format		format;
	media_codec_info	codecInfo;
	std::vector<uint8>	decodedData;
	int64				frameCount;
	bigtime_t			duration;
};


class MediaExtractor {
public:
							MediaExtractor();

	status_t				InitFromDataIO(BDataIO* source,
								const char* sourceName);
	status_t				InitCheck() const;
	int32					CountTracks() const;
	const DecodedTrackInfo*		TrackInfoAt(int32 index) const;
	const media_file_format&	FileFormat() const;
	const char*				Copyright() const;

private:
	status_t				_DecodeMemory(const void* data, size_t size,
								const char* sourceName);
	void					_InitFileFormat(const char* sourceName);

private:
	status_t				fInitStatus;
	media_file_format		fFileFormat;
	std::vector<DecodedTrackInfo>	fTracks;
};


class MediaWriter {
};


class MediaStreamer {
};

} }


#endif