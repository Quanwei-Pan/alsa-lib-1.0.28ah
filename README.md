# alsa-lib-1.0.28ah

Audio hack using alsa-lib.It can produce required_time longth date.

Added APIS:
 | int snd_dummy_set_trigger(snd_pcm_dummy_read_trigger_t enable)				                      
 | int snd_dummy_init(char * file_name, int mem_size_inbyte)					                           
 | int snd_dummy_generate_file(int time_in_sec)							                                   

Including Path:
  #include "alsa/asoundlib.h"
Linking Association：
 -L ./libs/libasound.so.2 ./libs/libasound.so.2.0.0   -wl,-rpath-link ./libs

ss.pan 
20170816
