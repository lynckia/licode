#include <string>

#include "MediaProcessor.h"

//UDPSocket *sock=NULL;
//UDPSocket *out=NULL;
std::string s;
unsigned short u;

MediaProcessor::MediaProcessor() {

	audioCoder = 0;
	audioDecoder = 0;
	videoCoder = 0;
	videoDecoder = 0;

	audioPackager = 0;
	audioUnpackager = 0;
	videoPackager = 0;
	videoUnpackager = 0;

	avcodec_register_all();
	av_register_all();
	avformat_network_init();
}

MediaProcessor::~MediaProcessor() {

	if (audioCoder == 1) {
		avcodec_close(aCoderContext);
		av_free(aCoderContext);
	}
	if (audioDecoder == 1) {
		avcodec_close(aDecoderContext);
		av_free(aDecoderContext);
	}

	if (videoCoder == 1) {
		avcodec_close(vCoderContext);
		av_free(vCoderContext);
		av_free(cPicture);
	}

	if (videoDecoder == 1) {
		avcodec_close(vDecoderContext);
		av_free(vDecoderContext);
		av_free(dPicture);
	}
}

bool MediaProcessor::initAudioCoder(const audioCodecInfo *audioCodec) {

	aCoder = avcodec_find_encoder(audioCodec->codec);
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
	aCoderContext->bit_rate = audioCodec->bitRate;
	aCoderContext->sample_rate = audioCodec->sampleRate;
	aCoderContext->channels = 1;

	if (avcodec_open2(aCoderContext, aCoder, NULL) < 0) {
		printf("Error al abrir el coder de audio");
		return false;
	}

	audioCoder = 1;
	return true;
}

bool MediaProcessor::initAudioDecoder(const audioCodecInfo *audioCodec) {

	aDecoder = avcodec_find_decoder(audioCodec->codec);
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
	aDecoderContext->bit_rate = audioCodec->bitRate;
	aDecoderContext->sample_rate = audioCodec->sampleRate;
	aDecoderContext->channels = 1;

	if (avcodec_open2(aDecoderContext, aDecoder, NULL) < 0) {
		printf("Error al abrir el decoder de audio");
		return false;
	}

	audioDecoder = 1;
	return true;

}

bool MediaProcessor::initVideoCoder(const videoCodecInfo *videoCodec) {

	vCoder = avcodec_find_encoder(videoCodec->codec);
	if (!vCoder) {
		printf("Encoder de vídeo no encontrado");
		return false;
	}

	vCoderContext = avcodec_alloc_context3(vCoder);
	if (!vCoderContext) {
		printf("Error de memoria en coder de vídeo");
		return false;
	}

	vCoderContext->bit_rate = videoCodec->bitRate;
	vCoderContext->bit_rate_tolerance = 1.1 * videoCodec->bitRate
			/ videoCodec->frameRate;
	vCoderContext->rc_max_rate = videoCodec->bitRate * 2;

	if (videoCodec->frameRate >= 1.0) {
		vCoderContext->rc_buffer_size = videoCodec->bitRate; // 1 second stored, in bps
	} else {
		vCoderContext->rc_buffer_size = 1.1 * videoCodec->bitRate
				/ videoCodec->frameRate;
	}

	vCoderContext->rc_buffer_aggressivity = 1.0;
	vCoderContext->gop_size = videoCodec->maxInter;
	vCoderContext->max_b_frames = 0;
	vCoderContext->me_method = ME_EPZS;

	vCoderContext->width = videoCodec->width;
	vCoderContext->height = videoCodec->height;
	vCoderContext->pix_fmt = PIX_FMT_YUV420P;
	vCoderContext->time_base = (AVRational) {1000, 1000*videoCodec->frameRate};

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

bool MediaProcessor::initVideoDecoder(const videoCodecInfo *videoCodec) {

	vDecoder = avcodec_find_decoder(videoCodec->codec);
	if (!vDecoder) {
		printf("Decoder de vídeo no encontrado");
		return false;
	}

	vDecoderContext = avcodec_alloc_context3(vDecoder);
	if (!vDecoderContext) {
		printf("Error de memoria en decoder de vídeo");
		return false;
	}

	vDecoderContext->width = videoCodec->width;
	vDecoderContext->height = videoCodec->height;

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

bool MediaProcessor::initAudioPackagerRTP(const RTPInfo *audioRTP) {

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
	aOutputFormat->audio_codec = audioRTP->codec;

	audioPackager = 1;
	return true;
}

bool MediaProcessor::initAudioUnpackagerRTP(const RTPInfo *audioRTP) {

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

	//aInputFormat->flags = AVFMT_NOFILE;
	aInputFormatContext->iformat = aInputFormat;

	audioUnpackager = 1;
	return true;
}

bool MediaProcessor::initVideoPackagerRTP(const RTPInfo *videoRTP) {
//
//	vOutputFormatContext = avformat_alloc_context();
//	if (!vOutputFormatContext){
//		printf("Memory Error al inicializar videoPackager");
//		return false;
//	}
//
//	vOutputFormat = av_guess_format("rtp",NULL,NULL);
//	if (vOutputFormat == NULL){
//	   printf("Could not guess format al inicializar videoPackager");
//	   return false;
//	}
//
//	vOutputFormatContext->oformat=vOutputFormat;
//	vOutputFormat->video_codec = videoRTP->codec;

	vRTPInfo = (RTPInfo*) videoRTP;
	vRTPInfo->seqNum = 0;
	vRTPInfo->ssrc = 5;

	videoPackager = 1;
	return true;
}

int readPacket(void *opaque, uint8_t *buf, int buf_size) {
	/*
	 if (sock==NULL){
	 sock = new UDPSocket(5004);
	 printf("Abierto socket\n");
	 }

	 int a = sock->recvFrom(buf, buf_size, s, u);
	 rtpHeader *h = (rtpHeader*)buf;
	 h->payloadtype = 33;
	 if (a>12){
	 buf = buf+12;
	 a-=12;
	 }
	 printf("Read packet %d, buf_size %d\n", a, buf_size);
	 */
	return 0;

}

int writePacket(void *opaque, uint8_t *buf, int buf_size) {

	printf("Write packet\n");

	return 0;

}
bool MediaProcessor::initVideoUnpackagerRTP(const RTPInfo *videoRTP) {

	vInputFormatContext = avformat_alloc_context();
	if (!vInputFormatContext) {
		printf("Memory Error al inicializar videoUnpackager");
		return false;
	}

	vInputFormat = av_find_input_format("rtp");
	if (vInputFormat == NULL) {
		printf("Could not find format al inicializar videoUnpackager");
		return false;
	}

//	vInputFormat->flags |= AVFMT_NOFILE ;
//	vInputFormatContext->flags |= AVFMT_NOFILE;
	vInputFormatContext->iformat = vInputFormat;

	int size = 15000;
	unsigned char *buff = (unsigned char*) av_malloc(size);

	AVIOContext *io = avio_alloc_context(buff, size, 0, NULL, &readPacket,
			&writePacket, NULL);

	vInputFormatContext->pb = io;
//	vInputFormatContext->flags |= AVFMT_FLAG_CUSTOM_IO;
	vInputFormatContext->video_codec_id = CODEC_ID_H264;
	printf("Config %s\n", avformat_configuration());
	int res = avformat_open_input(&vInputFormatContext, "", vInputFormat, NULL);
	printf(
			"INPUT OPEN********************************************************%d \n",
			res);
	AVInputFormat *fmt = NULL;

	videoUnpackager = 1;
	return true;

}

int MediaProcessor::encodeAudio(char *inBuff, int nSamples, char *outBuff) {

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

int MediaProcessor::decodeAudio(char *inBuff, int inBuffLen, char *outBuff) {

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

int MediaProcessor::encodeVideo(char *inBuff, int inBuffLen, char *outBuff,
		int outBuffLen) {

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
		printf("buffer smaller than minimum sizeS");
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

int MediaProcessor::decodeVideo(char *inBuff, int inBuffLen, char *outBuff,
		int outBuffLen, int *gotFrame) {

	if (videoDecoder == 0) {
		printf("No se han inicializado los parámetros del videoDecoder");
		return -1;
	}

	*gotFrame = 0;

	AVPacket avpkt;

	avpkt.data = (unsigned char*) inBuff;
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

	unsigned char *lum = (unsigned char*) outBuff;
	unsigned char *cromU = (unsigned char*) outBuff + outSize;
	unsigned char *cromV = (unsigned char*) outBuff + outSize + outSize / 4;

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

int MediaProcessor::packageAudioRTP(char *inBuff, int inBuffLen,
		char *outBuff) {

	if (audioPackager == 0) {
		printf("No se ha inicializado el codec de output audio RTP");
		return -1;
	}

	AVIOContext *c = avio_alloc_context((unsigned char*) outBuff, 2000, 1, NULL,
			NULL, NULL, NULL);

	aOutputFormatContext->pb = c;
	aOutputFormatContext->flags = AVFMT_NOFILE;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = (unsigned char*) inBuff;
	pkt.size = inBuffLen;

	int ret = av_write_frame(aOutputFormatContext, &pkt);

	av_free_packet(&pkt);
	av_free(c);

	return ret;
}

int MediaProcessor::unpackageAudioRTP(char *inBuff, int inBuffLen,
		char *outBuff) {

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

int MediaProcessor::packageVideoRTP(char *inBuff, int inBuffLen,
		char *outBuff) {

	if (videoPackager == 0) {
		printf("No se ha inicailizado el codec de output vídeo RTP");
		return -1;
	}

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
	head->seqnum = htons(vRTPInfo->seqNum++);
	head->timestamp = htonl(millis);
	head->ssrc = htonl(vRTPInfo->ssrc);
	head->payloadtype = 34; //32;

	outBuff += RTP_HEADER_LEN;

	memcpy(outBuff, inBuff, inBuffLen);

//	AVIOContext *c = avio_alloc_context((unsigned char*)outBuff, l, 1, NULL, NULL, NULL, NULL);
//
//	vOutputFormatContext->pb = c;
//	vOutputFormatContext->flags = AVFMT_NOFILE;
//
//	AVPacket pkt;
//	av_init_packet(&pkt);
//	pkt.data = (unsigned char*)inBuff;
//	pkt.size = inBuffLen;
//
//	int ret = av_write_frame(vOutputFormatContext,&pkt);
//
//	av_free_packet(&pkt);
//	av_free(c);

	return l;
}

int MediaProcessor::unpackageVideoRTP(char *inBuff, int inBuffLen,
		char *outBuff, int *gotFrame) {
	/*
	 if (videoUnpackager == 0) {
	 printf("No se ha inicailizado el codec de input vídeo RTP");
	 return -1;
	 }
	 */
	*gotFrame = 0;

	rtpHeader* head = (rtpHeader*) inBuff;

	int sec = ntohs(head->seqnum);
	int ssrc = ntohl(head->ssrc);
	unsigned long time = ntohl(head->timestamp);
	if (ssrc!= 55543)
		return -1;
	int l = inBuffLen - RTP_HEADER_LEN;

	inBuff += RTP_HEADER_LEN;
	vp8RtpHeader* vp8h = (vp8RtpHeader*) outBuff;
	printf("R: %u, PartID %u , X %u \n", vp8h->R, vp8h->partId, vp8h->X);
	//l--;
	//inBuff++;

	memcpy(outBuff, inBuff, l);


	if (head->marker) {
		*gotFrame = 1;
	}

	return l;
//
//	if (avformat_find_stream_info(vInputFormatContext, NULL) < 0) {
//		return -1;
//	}
//
//	AVPacket pkt;
//	av_init_packet(&pkt);
//
//	//aInputFormatContext->iformat->read_packet(aInputFormatContext, &pkt);
//
//	int p = av_read_frame(vInputFormatContext, &pkt);
//	printf("Leido frame %d\n", p);
//
//	outBuff = (char*) pkt.data;
//
//	int se = pkt.size;
//
//	av_free_packet(&pkt);
//
//	return se;

}

