#include <string>

#include "MediaProcessor.h"

//UDPSocket *sock=NULL;
//UDPSocket *out=NULL;

namespace erizo {

InputProcessor::InputProcessor() {

	audioDecoder = 0;
	videoDecoder = 0;

	audioUnpackager = 0;
	videoUnpackager = 0;
	gotUnpackagedFrame_ = false;
	upackagedSize_ = 0;

	decodedBuffer_ = NULL;

	avcodec_register_all();
	av_register_all();
}

InputProcessor::~InputProcessor() {
	if (audioDecoder == 1) {
		avcodec_close(aDecoderContext);
		av_free(aDecoderContext);
	}

	if (videoDecoder == 1) {
		avcodec_close(vDecoderContext);
		av_free(vDecoderContext);
		av_free(dPicture);
	}
	if (decodedBuffer_ != NULL) {
		free(decodedBuffer_);
	}
}

int InputProcessor::init(const MediaInfo &info, RawDataReceiver* receiver) {
	this->mediaInfo = info;
	this->rawReceiver_ = receiver;

	if (mediaInfo.hasVideo) {
		mediaInfo.videoCodec.codec = CODEC_ID_VP8;
		decodedBuffer_ = (unsigned char*) malloc(
				info.videoCodec.width * info.videoCodec.height * 3 / 2);
		unpackagedBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
		memset(decodedBuffer_, 0,
				info.videoCodec.width * info.videoCodec.height * 3 / 2);
		this->initVideoDecoder();
		this->initVideoUnpackager();
	}

	return 0;
}

int InputProcessor::receiveAudioData(char* buf, int len) {
	return 3;
}
int InputProcessor::receiveVideoData(char* buf, int len) {

	if (videoUnpackager && videoDecoder) {
		int ret = unpackageVideo(reinterpret_cast<unsigned char*>(buf), len,
				unpackagedBuffer_, &gotUnpackagedFrame_);
		upackagedSize_ += ret;
		unpackagedBuffer_ += ret;

		if (gotUnpackagedFrame_) {

			unpackagedBuffer_ -= upackagedSize_;

			printf("Tengo un frame desempaquetado!! Size = %d\n",
					upackagedSize_);

			int c;
			int gotDecodedFrame = 0;

			c = this->decodeVideo(unpackagedBuffer_, upackagedSize_,
					decodedBuffer_, 640 * 480 * 3 / 2, &gotDecodedFrame);

			upackagedSize_ = 0;
			gotUnpackagedFrame_ = 0;

			printf("Bytes dec = %d\n", c);
			if (gotDecodedFrame) {
				printf("Tengo un frame decodificado!!\n");
				gotDecodedFrame = 0;
				rawReceiver_->receiveRawData(decodedBuffer_, c);
				memset(unpackagedBuffer_, 0, 50000);

//				unsigned char *buffSend = (unsigned char*) malloc(c);
//				memcpy(buffSend, decodedBuffer_, c);

			}

		}
		return 3;
	}
	return 7;
}

bool InputProcessor::initAudioDecoder() {

	aDecoder = avcodec_find_decoder(mediaInfo.audioCodec.codec);
	if (!aDecoder) {
		printf("Decoder de audio no encontrado");
		return false;
	}

	aDecoderContext = avcodec_alloc_context3(aDecoder);
	if (!aDecoderContext) {
		printf("Error de memoria en decoder de audio");
		return false;
	}

	aDecoderContext->sample_fmt = AV_SAMPLE_FMT_S16;
	aDecoderContext->bit_rate = mediaInfo.audioCodec.bitRate;
	aDecoderContext->sample_rate = mediaInfo.audioCodec.sampleRate;
	aDecoderContext->channels = 1;

	if (avcodec_open2(aDecoderContext, aDecoder, NULL) < 0) {
		printf("Error al abrir el decoder de audio");
		return false;
	}

	audioDecoder = 1;
	return true;

}

bool InputProcessor::initVideoDecoder() {

	videoCodecInfo& videoCodec = mediaInfo.videoCodec;
	vDecoder = avcodec_find_decoder(videoCodec.codec);
	if (!vDecoder) {
		printf("Decoder de vídeo no encontrado");
		return false;
	}

	vDecoderContext = avcodec_alloc_context3(vDecoder);
	if (!vDecoderContext) {
		printf("Error de memoria en decoder de vídeo");
		return false;
	}

	vDecoderContext->width = videoCodec.width;
	vDecoderContext->height = videoCodec.height;

	if (avcodec_open2(vDecoderContext, vDecoder, NULL) < 0) {
		printf("Error al abrir el decoder de vídeo");
		return false;
	}

	dPicture = avcodec_alloc_frame();
	if (!dPicture) {
		printf("Error de memoria en frame del decoder de vídeo");
		return false;
	}

	videoDecoder = 1;
	return true;
}

bool InputProcessor::initAudioUnpackager() {

	if (mediaInfo.proccessorType != RTP_ONLY) {
		aInputFormatContext = avformat_alloc_context();
		if (!aInputFormatContext) {
			printf("Memory Error al inicializar audioUnpackager");
			return false;
		}

		aInputFormat = av_find_input_format("rtp");
		if (aInputFormat == NULL) {
			printf("Could not guess format al inicializar audioUnpackager");
			return false;
		}

		aInputFormatContext->iformat = aInputFormat;

		audioUnpackager = 1;
	}
	return true;
}

bool InputProcessor::initVideoUnpackager() {
	if (mediaInfo.proccessorType == RTP_ONLY) {
		printf("Unpackaging from RTP\n");
	} else {

	}
	videoUnpackager = 1;
	return true;

}

int InputProcessor::decodeAudio(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff) {

	if (audioDecoder == 0) {
		printf("No se han inicializado los parámetros del audioDecoder\n");
		return -1;
	}

	AVPacket avpkt;
	int outSize;
	int decSize = 0;
	int len = -1;
	uint8_t *decBuff = (uint8_t*) malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	av_init_packet(&avpkt);
	avpkt.data = (unsigned char*) inBuff;
	avpkt.size = inBuffLen;

	while (avpkt.size > 0) {

		outSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;

		//Puede fallar. Cogido de libavcodec/utils.c del paso de avcodec_decode_audio3 a avcodec_decode_audio4
		//avcodec_decode_audio3(aDecoderContext, (short*)decBuff, &outSize, &avpkt);

		AVFrame frame;
		int got_frame = 0;

		aDecoderContext->get_buffer = avcodec_default_get_buffer;
		aDecoderContext->release_buffer = avcodec_default_release_buffer;

		len = avcodec_decode_audio4(aDecoderContext, &frame, &got_frame,
				&avpkt);

		if (len >= 0 && got_frame) {
			int plane_size;
			//int planar = av_sample_fmt_is_planar(aDecoderContext->sample_fmt);
			int data_size = av_samples_get_buffer_size(&plane_size,
					aDecoderContext->channels, frame.nb_samples,
					aDecoderContext->sample_fmt, 1);
			if (outSize < data_size) {
				printf(
						"output buffer size is too small for the current frame\n");
				return AVERROR(EINVAL);
			}

			memcpy(decBuff, frame.extended_data[0], plane_size);

			/* Si hay más de un canal
			 if (planar && aDecoderContext->channels > 1) {
			 uint8_t *out = ((uint8_t *)decBuff) + plane_size;
			 for (int ch = 1; ch < aDecoderContext->channels; ch++) {
			 memcpy(out, frame.extended_data[ch], plane_size);
			 out += plane_size;
			 }
			 }
			 */

			outSize = data_size;
		} else {
			outSize = 0;
		}

		if (len < 0) {
			printf("Error al decodificar audio\n");
			free(decBuff);
			return -1;
		}

		avpkt.size -= len;
		avpkt.data += len;

		if (outSize <= 0) {
			continue;
		}

		memcpy(outBuff, decBuff, outSize);
		outBuff += outSize;
		decSize += outSize;
	}

	free(decBuff);

	if (outSize <= 0) {
		printf("Error de decodificación de audio debido a tamaño incorrecto");
		return -1;
	}

	return decSize;

}

int InputProcessor::decodeVideo(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff, int outBuffLen, int* gotFrame) {

	if (videoDecoder == 0) {
		printf("No se han inicializado los parámetros del videoDecoder");
		return -1;
	}

	*gotFrame = 0;

	AVPacket avpkt;

	avpkt.data = inBuff;
	avpkt.size = inBuffLen;

	int got_picture;
	int len;

	while (avpkt.size > 0) {

		len = avcodec_decode_video2(vDecoderContext, dPicture, &got_picture,
				&avpkt);

		if (len < 0) {
			printf("Error al decodificar frame de vídeo");
			return -1;
		}

		if (got_picture) {
			*gotFrame = 1;
			goto decoding;
		}
		avpkt.size -= len;
		avpkt.data += len;
	}

	if (!got_picture) {
		printf("Aún no tengo frame");
		return -1;
	}

	decoding:

	int outSize = vDecoderContext->height * vDecoderContext->width;

	if (outBuffLen < (outSize * 3 / 2)) {
		printf("No se ha rellenado el buffer???");
		return outSize * 3 / 2;
	}

	unsigned char *lum = outBuff;
	unsigned char *cromU = outBuff + outSize;
	unsigned char *cromV = outBuff + outSize + outSize / 4;

	unsigned char *src = NULL;
	int src_linesize, dst_linesize;

	src_linesize = dPicture->linesize[0];
	dst_linesize = vDecoderContext->width;
	src = dPicture->data[0];

	for (int i = vDecoderContext->height; i > 0; i--) {
		memcpy(lum, src, dst_linesize);
		lum += dst_linesize;
		src += src_linesize;
	}

	src_linesize = dPicture->linesize[1];
	dst_linesize = vDecoderContext->width / 2;
	src = dPicture->data[1];

	for (int i = vDecoderContext->height / 2; i > 0; i--) {
		memcpy(cromU, src, dst_linesize);
		cromU += dst_linesize;
		src += src_linesize;
	}

	src_linesize = dPicture->linesize[2];
	dst_linesize = vDecoderContext->width / 2;
	src = dPicture->data[2];

	for (int i = vDecoderContext->height / 2; i > 0; i--) {
		memcpy(cromV, src, dst_linesize);
		cromV += dst_linesize;
		src += src_linesize;
	}

	return outSize * 3 / 2;
}

int InputProcessor::unpackageAudio(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff) {

	/*
	 if (audioUnpackager == 0) {
	 printf("No se ha inicializado el codec de input audio RTP");
	 return -1;
	 }

	 AVIOContext *c = avio_alloc_context((unsigned char*)inBuff, inBuffLen, 0, NULL, NULL, NULL, NULL);

	 aInputFormatContext->pb = c;
	 //aInputFormatContext->flags = AVFMT_NOFILE;
	 aInputFormatContext->flags = AVFMT_FLAG_CUSTOM_IO;

	 if (avformat_find_stream_info(aInputFormatContext, NULL) < 0) {
	 return -1;
	 }

	 AVPacket pkt;
	 av_init_packet(&pkt);

	 //aInputFormatContext->iformat->read_packet(aInputFormatContext, &pkt);




	 av_read_frame(aInputFormatContext, &pkt);


	 outBuff = (char*)pkt.data;

	 int se =  pkt.size;

	 av_free_packet(&pkt);
	 av_free(c);

	 return se;
	 */

	int l = inBuffLen - RTP_HEADER_LEN;

	inBuff += RTP_HEADER_LEN;
	memcpy(outBuff, inBuff, l);

	return l;
}

int InputProcessor::unpackageVideo(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff, int *gotFrame) {

	if (videoUnpackager == 0) {
		printf("Unpackager not correctly initialized");
		return -1;
	}
	*gotFrame = 0;
	if (mediaInfo.proccessorType == RTP_ONLY) {
		rtpHeader* head = (rtpHeader*) inBuff;

		int sec = ntohs(head->seqnum);
		int ssrc = ntohl(head->ssrc);
		unsigned long time = ntohl(head->timestamp);
		printf("PT %d, ssrc %u, extension %d\n", head->payloadtype, ssrc,
				head->extension);
		if (ssrc != 55543 /*&& head->payloadtype!=101*/) {
			return -1;
		}
		if (head->payloadtype != 100) {
			printf("EEEEEEEEEEEEEEOOOOOOOOOOOOOOOOOOOOOOOO %d\n\n\n",
					head->payloadtype);
			return -1;
		}
		int l = inBuffLen - RTP_HEADER_LEN;
		inBuff += RTP_HEADER_LEN;
//		vp8RtpHeader* vphead = (vp8RtpHeader*) inBuff;
//		printf("MIO X: %u , N:%u PartID %u\n", vphead->X, vphead->N,
//			vphead->partId);

		erizo::RTPPayloadVP8* parsed = pars.parseVP8((unsigned char*) inBuff,
				l);
		printf("l : %d, parsedDatalength %u\n", l, parsed->dataLength);
//		l--;
//		inBuff++;

//	memcpy(outBuff, inBuff, parsed->dataLength);
		memcpy(outBuff, parsed->data, parsed->dataLength);
		if (head->marker) {
			printf("Marker\n");
			*gotFrame = 1;
		}

//	return l;
		return parsed->dataLength;
	} else {

		if (avformat_find_stream_info(vInputFormatContext, NULL) < 0) {
			return -1;
		}

		AVPacket pkt;
		av_init_packet(&pkt);

		//aInputFormatContext->iformat->read_packet(aInputFormatContext, &pkt);

		int p = av_read_frame(vInputFormatContext, &pkt);
		printf("Leido frame %d\n", p);

		outBuff = pkt.data;

		int se = pkt.size;

		av_free_packet(&pkt);

		return se;

	}

}

OutputProcessor::OutputProcessor() {

	audioCoder = 0;
	videoCoder = 0;

	audioPackager = 0;
	videoPackager = 0;

	encodedBuffer_ = NULL;
	packagedBuffer_ = NULL;

	avcodec_register_all();
	av_register_all();
}

OutputProcessor::~OutputProcessor() {

	if (audioCoder == 1) {
		avcodec_close(aCoderContext);
		av_free(aCoderContext);
	}

	if (videoCoder == 1) {
		avcodec_close(vCoderContext);
		av_free(vCoderContext);
		av_free(cPicture);
	}
	if (encodedBuffer_) {
		free(encodedBuffer_);
	}
	if (packagedBuffer_) {
		free(packagedBuffer_);
	}
}

int OutputProcessor::init(const MediaInfo &info) {
	this->mediaInfo = info;

	encodedBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);
	packagedBuffer_ = (unsigned char*) malloc(UNPACKAGED_BUFFER_SIZE);

//	videoCodecInfo c;
//	c.codec = CODEC_ID_H264;
//	c.width = 640;
//	c.height = 480;
//	c.frameRate = 24;
//	c.bitRate = 1024;
//	c.maxInter = 0;
	this->mediaInfo.videoCodec.codec = CODEC_ID_H263P;

	this->initVideoCoder();
	this->initVideoPackager();
	return 0;
}

void OutputProcessor::receiveRawData(unsigned char* data, int len) {
	int a = this->encodeVideo(data, len, encodedBuffer_,
			UNPACKAGED_BUFFER_SIZE);

	printf("\nBytes codificados = %d", a);

	int b = this->packageVideo(encodedBuffer_, a, packagedBuffer_);
	printf("\nBytes empaquetados = %d", b);
}

bool OutputProcessor::initAudioCoder() {

	aCoder = avcodec_find_encoder(mediaInfo.audioCodec.codec);
	if (!aCoder) {
		printf("Encoder de audio no encontrado");
		return false;
	}

	aCoderContext = avcodec_alloc_context3(aCoder);
	if (!aCoderContext) {
		printf("Error de memoria en coder de audio");
		return false;
	}

	aCoderContext->sample_fmt = AV_SAMPLE_FMT_S16;
	aCoderContext->bit_rate = mediaInfo.audioCodec.bitRate;
	aCoderContext->sample_rate = mediaInfo.audioCodec.sampleRate;
	aCoderContext->channels = 1;

	if (avcodec_open2(aCoderContext, aCoder, NULL) < 0) {
		printf("Error al abrir el coder de audio");
		return false;
	}

	audioCoder = 1;
	return true;
}

bool OutputProcessor::initVideoCoder() {

	vCoder = avcodec_find_encoder(mediaInfo.videoCodec.codec);
	if (!vCoder) {
		printf("Encoder de vídeo no encontrado");
		return false;
	}

	vCoderContext = avcodec_alloc_context3(vCoder);
	if (!vCoderContext) {
		printf("Error de memoria en coder de vídeo");
		return false;
	}

	vCoderContext->bit_rate = mediaInfo.videoCodec.bitRate;
	vCoderContext->bit_rate_tolerance = 1.1 * mediaInfo.videoCodec.bitRate
			/ mediaInfo.videoCodec.frameRate;
	vCoderContext->rc_max_rate = mediaInfo.videoCodec.bitRate * 2;

	if (mediaInfo.videoCodec.frameRate >= 1.0) {
		vCoderContext->rc_buffer_size = mediaInfo.videoCodec.bitRate; // 1 second stored, in bps
	} else {
		vCoderContext->rc_buffer_size = 1.1 * mediaInfo.videoCodec.bitRate
				/ mediaInfo.videoCodec.frameRate;
	}

	vCoderContext->rc_buffer_aggressivity = 1.0;
	vCoderContext->gop_size = mediaInfo.videoCodec.maxInter;
	vCoderContext->max_b_frames = 0;
	vCoderContext->me_method = ME_EPZS;

	vCoderContext->width = mediaInfo.videoCodec.width;
	vCoderContext->height = mediaInfo.videoCodec.height;
	vCoderContext->pix_fmt = PIX_FMT_YUV420P;
	vCoderContext->time_base =
			(AVRational) {1000, 1000*mediaInfo.videoCodec.frameRate};

	if (avcodec_open2(vCoderContext, vCoder, NULL) < 0) {
		printf("Error al abrir el decoder de vídeo");
		return false;
	}

	cPicture = avcodec_alloc_frame();
	if (!cPicture) {
		printf("Error de memoria en frame del coder de vídeo");
		return false;
	}

	videoCoder = 1;
	return true;
}

bool OutputProcessor::initAudioPackager() {

	aOutputFormatContext = avformat_alloc_context();
	if (!aOutputFormatContext) {
		printf("Memory Error al inicializar audioPackager");
		return false;
	}

	aOutputFormat = av_guess_format("rtp", NULL, NULL);
	if (aOutputFormat == NULL) {
		printf("Could not guess format al inicializar audioPackager");
		return false;
	}

	aOutputFormatContext->oformat = aOutputFormat;
	aOutputFormat->audio_codec = mediaInfo.audioCodec.codec;

	audioPackager = 1;
	return true;
}

bool OutputProcessor::initVideoPackager() {
	if (mediaInfo.proccessorType == RTP_ONLY) {
		printf("RTP ONLY OutputProcessor\n");
	} else {
		vOutputFormatContext = avformat_alloc_context();
		if (!vOutputFormatContext) {
			printf("videoPackager: Error creating avformat context \n");
			return false;
		}

		vOutputFormat = av_guess_format(mediaInfo.url.c_str(), NULL, NULL);
		if (vOutputFormat == NULL) {
			printf("videoPackager: Could not guess format");
			return false;
		}

		vOutputFormatContext->oformat = vOutputFormat;
		vOutputFormat->video_codec = mediaInfo.videoCodec.codec;
	}
	videoPackager = 1;
	return true;
}

int OutputProcessor::packageAudio(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff) {

	if (audioPackager == 0) {
		printf("No se ha inicializado el codec de output audio RTP");
		return -1;
	}

	AVIOContext *c = avio_alloc_context((unsigned char*) outBuff, 2000, 1, NULL,
			NULL, NULL, NULL);

//	aOutputFormatContext->pb = c;
//	aOutputFormatContext->flags = AVFMT_NOFILE;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = (unsigned char*) inBuff;
	pkt.size = inBuffLen;

	int ret = av_write_frame(aOutputFormatContext, &pkt);

	av_free_packet(&pkt);
	av_free(c);

	return ret;
}

int OutputProcessor::packageVideo(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff) {

	if (videoPackager == 0) {
		printf("No se ha inicailizado el codec de output vídeo RTP");
		return -1;
	}

	if (mediaInfo.proccessorType == RTP_ONLY) {

		int l = inBuffLen + RTP_HEADER_LEN;

		rtpHeader * head = (rtpHeader*) outBuff;

		timeval time;
		gettimeofday(&time, NULL);
		long millis = (time.tv_sec * 1000) + (time.tv_usec / 1000);

		head->version = 2; //v = 2
		head->extension = 0;
		head->marker = 1; //n?0:1;
		head->padding = 0;
		head->cc = 0;
//	head->seqnum = htons(vRTPInfo->seqNum++);
		head->timestamp = htonl(millis);
		head->ssrc = htonl(vRTPInfo->ssrc);
		head->payloadtype = 34; //32;

		outBuff += RTP_HEADER_LEN;

		memcpy(outBuff, inBuff, inBuffLen);
		return l;

	} else {
//		AVIOContext *c = avio_alloc_context((unsigned char*) outBuff, l, 1,
//				NULL, NULL, NULL, NULL);
//
//		vOutputFormatContext->pb = c;
//		vOutputFormatContext->flags = AVFMT_NOFILE;

		AVPacket pkt;
		av_init_packet(&pkt);
		pkt.data = (unsigned char*) inBuff;
		pkt.size = inBuffLen;

		int ret = av_write_frame(vOutputFormatContext, &pkt);

		av_free_packet(&pkt);
//		av_free(c);
	}

}

int OutputProcessor::encodeAudio(unsigned char* inBuff, int nSamples,
		unsigned char* outBuff) {

	if (audioCoder == 0) {
		printf("No se han inicializado los parámetros del audioCoder");
		return -1;
	}

	//Puede fallar. Revisar samples. Cogido de libavcodec/utils.c del paso de avcodec_encode_audio a avcodec_encode_audio2
	//avcodec_encode_audio(aCoderContext, (unsigned char*)outBuff, nSamples*2, (short*)inBuff);

	AVPacket pkt;
	AVFrame frame0;
	AVFrame *frame;
	int ret, samples_size, got_packet;

	av_init_packet(&pkt);
	pkt.data = (unsigned char*) inBuff;
	pkt.size = nSamples * 2;

	frame = &frame0;
	avcodec_get_frame_defaults(frame);

	frame->nb_samples = nSamples;

	samples_size = av_samples_get_buffer_size(NULL, aCoderContext->channels,
			frame->nb_samples, aCoderContext->sample_fmt, 1);

	if ((ret = avcodec_fill_audio_frame(frame, aCoderContext->channels,
			aCoderContext->sample_fmt, (const uint8_t *) inBuff, samples_size,
			1)))
		return ret;

	frame->pts = AV_NOPTS_VALUE;
	//aCoderContext->internal->sample_count += frame->nb_samples;

	got_packet = 0;

	ret = avcodec_encode_audio2(aCoderContext, &pkt, frame, &got_packet);

	if (!ret && got_packet && aCoderContext->coded_frame) {
		aCoderContext->coded_frame->pts = pkt.pts;
		aCoderContext->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
	}
	//ff_packet_free_side_data(&pkt);

	if (frame && frame->extended_data != frame->data)
		av_freep(&frame->extended_data);

	return ret;

}

int OutputProcessor::encodeVideo(unsigned char* inBuff, int inBuffLen,
		unsigned char* outBuff, int outBuffLen) {

	if (videoCoder == 0) {
		printf("No se han inicializado los parámetros del videoCoder");
		return -1;
	}

	int size = vCoderContext->width * vCoderContext->height;

	cPicture->pts = AV_NOPTS_VALUE;
	cPicture->data[0] = (unsigned char*) inBuff;
	cPicture->data[1] = (unsigned char*) inBuff + size;
	cPicture->data[2] = (unsigned char*) inBuff + size + size / 4;
	cPicture->linesize[0] = vCoderContext->width;
	cPicture->linesize[1] = vCoderContext->width / 2;
	cPicture->linesize[2] = vCoderContext->width / 2;

	AVPacket pkt;
	int ret = 0;
	int got_packet = 0;

	if (outBuffLen < FF_MIN_BUFFER_SIZE) {
		printf("buffer smaller than minimum size");
		return -1;
	}

	av_init_packet(&pkt);
	pkt.data = (unsigned char*) outBuff;
	pkt.size = outBuffLen;

	ret = avcodec_encode_video2(vCoderContext, &pkt, cPicture, &got_packet);

	if (!ret && got_packet && vCoderContext->coded_frame) {
		vCoderContext->coded_frame->pts = pkt.pts;
		vCoderContext->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
	}

	/* free any side data since we cannot return it */
	if (pkt.side_data_elems > 0) {
		int i;
		for (i = 0; i < pkt.side_data_elems; i++)
			av_free(pkt.side_data[i].data);
		av_freep(&pkt.side_data);
		pkt.side_data_elems = 0;
	}

	return ret ? ret : pkt.size;

}
} /* namespace erizo */
