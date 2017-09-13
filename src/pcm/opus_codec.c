#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "opus_codec.h"
#include "opus/opus.h"
#include "opus/opus_types.h"

#define MAX_PACKET 1500

static void int_to_char(opus_uint32 i, unsigned char ch[4])
{
	ch[0] = i>>24;
	ch[1] = (i>>16)&0xFF;
	ch[2] = (i>>8)&0xFF;
	ch[3] = i&0xFF;
}

int mi_opus(char *input, int inputsize, char *output, int *outputsize, float sleep_time)
{
	int err;
	int offset = 0,offset1 = 0,readsize = 0, leftsize = inputsize;
	OpusEncoder *enc = NULL;
	int len[2];
	opus_int32 sampling_rate = 16000;
	int frame_size = sampling_rate/50;
	int channels = 1;
	opus_int32 bitrate_bps=64000;
	unsigned char *data[2];
	unsigned char *fbytes;
	int max_payload_bytes = MAX_PACKET;
	int use_inbandfec = 0;
	int use_dtx = 0;
	int packet_loss_perc = 0;
	opus_int32 count=0, count_act=0;
	int k;
	opus_int32 skip=0;
	int stop=0;
	short *in, *out;
	double bits=0.0, bits_max=0.0, bits_act=0.0, bits2=0.0, nrg;
	double tot_samples=0;
	opus_uint64 tot_in, tot_out;
	int lost = 0, lost_prev = 1;
	int toggle = 0;
	opus_uint32 enc_final_range[2];
	int max_frame_size = 48000;
	size_t num_read;
	int curr_read=0;
	int sweep_bps = 0;
	int random_framesize=0, newsize=0;
	int random_fec=0;
	const int (*mode_list)[4]=NULL;
	int nb_modes_in_list=0;
	int curr_mode=0;
	int curr_mode_count=0;
	int mode_switch_time = 48000;
	int nb_encoded=0;
	int remaining=0;
	int delayed_decision=0;
	tot_in=tot_out=0;

	if (max_payload_bytes < 0 || max_payload_bytes > MAX_PACKET)
	{
		fprintf (stderr, "max_payload_bytes must be between 0 and %d\n",
				MAX_PACKET);
		return EXIT_FAILURE;
	}

	if (mode_list)
	{
		mode_switch_time = inputsize/sizeof(short)/channels/nb_modes_in_list;
	}
	enc = opus_encoder_create(sampling_rate, channels, OPUS_APPLICATION_VOIP, &err);
	if (err != OPUS_OK)
	{
		fprintf(stderr, "Cannot create encoder: %s\n", opus_strerror(err));
		return EXIT_FAILURE;
	}
	opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_bps));
	opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(OPUS_AUTO));
	opus_encoder_ctl(enc, OPUS_SET_VBR(0));
	opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(0));
	opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
	opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(use_inbandfec));
	opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(OPUS_AUTO));
	opus_encoder_ctl(enc, OPUS_SET_DTX(use_dtx));
	opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(packet_loss_perc));
	opus_encoder_ctl(enc, OPUS_GET_LOOKAHEAD(&skip));
	opus_encoder_ctl(enc, OPUS_SET_LSB_DEPTH(16));
	opus_encoder_ctl(enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_ARG));

	in = (short*)malloc(max_frame_size*channels*sizeof(short));
	out = (short*)malloc(max_frame_size*channels*sizeof(short));
	/* We need to allocate for 16-bit PCM data, but we store it as unsigned char. */
	fbytes = (unsigned char*)malloc(max_frame_size*channels*sizeof(short));
	data[0] = (unsigned char*)calloc(max_payload_bytes,sizeof(unsigned char));
	if ( use_inbandfec ) {
		data[1] = (unsigned char*)calloc(max_payload_bytes,sizeof(unsigned char));
	}
	while (!stop)
	{
		if (random_framesize && rand()%20==0)
		{
			newsize = rand()%6;
			switch(newsize)
			{
			case 0: newsize=sampling_rate/400; break;
			case 1: newsize=sampling_rate/200; break;
			case 2: newsize=sampling_rate/100; break;
			case 3: newsize=sampling_rate/50; break;
			case 4: newsize=sampling_rate/25; break;
			case 5: newsize=3*sampling_rate/50; break;
			}
			while (newsize < sampling_rate/25 && bitrate_bps-abs(sweep_bps) <= 3*12*sampling_rate/newsize)
				newsize*=2;
			if (newsize < sampling_rate/100 && frame_size >= sampling_rate/100)
			{
				opus_encoder_ctl(enc, OPUS_SET_FORCE_MODE(MODE_CELT_ONLY));
			} else {
				frame_size = newsize;
			}
		}
		if (random_fec && rand()%30==0)
		{
			opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(rand()%4==0));
		}
		int i;
		if (mode_list!=NULL)
		{
			opus_encoder_ctl(enc, OPUS_SET_BANDWIDTH(mode_list[curr_mode][1]));
			opus_encoder_ctl(enc, OPUS_SET_FORCE_MODE(mode_list[curr_mode][0]));
			opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(mode_list[curr_mode][3]));
			frame_size = mode_list[curr_mode][2];
		}

		readsize = (leftsize/2 > frame_size) ? frame_size : leftsize/2;
		memcpy(fbytes, input+offset, sizeof(short)*channels * readsize);
		num_read = readsize;
		offset += sizeof(short)*channels * readsize;
		leftsize -= readsize*sizeof(short)*channels;
		curr_read = (int)num_read;
		tot_in += curr_read;
		for(i=0; i<curr_read*channels; i++)
		{
			opus_int32 s;
			s=fbytes[2*i+1]<<8|fbytes[2*i];
			s=((s&0xFFFF)^0x8000)-0x8000;
			in[i+remaining*channels]=s;
		}
		if (curr_read+remaining < frame_size)
		{
			for (i=(curr_read+remaining)*channels; i<frame_size*channels; i++)
				in[i] = 0;
			stop = 1;
		}
		len[toggle] = opus_encode(enc, in, frame_size, data[toggle], max_payload_bytes);
		nb_encoded = opus_packet_get_samples_per_frame(data[toggle], sampling_rate)*opus_packet_get_nb_frames(data[toggle], len[toggle]);
		remaining = frame_size-nb_encoded;
		for(i=0; i<remaining*channels; i++)
			in[i] = in[nb_encoded*channels+i];
		if (sweep_bps!=0)
		{
			bitrate_bps += sweep_bps;
			/* safety */
			if (bitrate_bps<1000)
				bitrate_bps = 1000;
			opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrate_bps));
		}
		opus_encoder_ctl(enc, OPUS_GET_FINAL_RANGE(&enc_final_range[toggle]));
		if (len[toggle] < 0)
		{
			fprintf (stderr, "opus_encode() returned %d\n", len[toggle]);
			return EXIT_FAILURE;
		}
		curr_mode_count += frame_size;
		if (curr_mode_count > mode_switch_time && curr_mode < nb_modes_in_list-1)
		{
			curr_mode++;
			curr_mode_count = 0;
		}
		unsigned char int_field[4];
		int_to_char(len[toggle], int_field);
		memcpy(output+offset1, int_field, 4);
		offset1 += 4;
		int_to_char(enc_final_range[toggle], int_field);
		memcpy(output+offset1, int_field, 4);
		offset1 += 4;

		memcpy(output+offset1, data[toggle], len[toggle]);
		offset1 += len[toggle];
		tot_samples += nb_encoded;
		usleep((int) 1000 * sleep_time);
		//usleep(0);
	}

	lost_prev = lost;
	if( count >= use_inbandfec ) {
		/* count bits */
		bits += len[toggle]*8;
		bits_max = ( len[toggle]*8 > bits_max ) ? len[toggle]*8 : bits_max;
		bits2 += len[toggle]*len[toggle]*64;
		nrg = 0.0;
		for ( k = 0; k < frame_size * channels; k++ ) {
			nrg += in[ k ] * (double)in[ k ];
		}
		nrg /= frame_size * channels;
		if( nrg > 1e5 ) {
			bits_act += len[toggle]*8;
			count_act++;
		}
	}
	count++;
	toggle = (toggle + use_inbandfec) & 1;

	*outputsize = offset1;
	count -= use_inbandfec;
	opus_encoder_destroy(enc);
	free(data[0]);
	if (use_inbandfec)
		free(data[1]);
	free(in);
	free(out);
	free(fbytes);
	return EXIT_SUCCESS;
}
