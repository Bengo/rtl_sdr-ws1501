#include <iostream>
#include <rtl-sdr.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <vector>
#include <math.h>
#include <string>
#include <bitset>
#include <sstream>



#define DEFAULT_SAMPLE_RATE 250000
#define DEFAULT_ASYNC_BUF_NUMBER	32
#define DEFAULT_BUF_LENGTH		(16 * 16384)
#define SETSIZE 4
using namespace std;

static int do_exit = 0;
static rtlsdr_dev_t *dev = NULL;
static uint32_t bytes_to_read = 0;


unsigned char* memblock;

string enteteThermometre = "2dd4a1430";
string enteteHumidite = "2dd4a1431";


static void sighandler(int signum)
{
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	rtlsdr_cancel_async(dev);
}

static void to_hex_str(string& binary_str, ostringstream& hex_str)
{
    size_t idx=0, size=binary_str.size();

    hex_str << hex ;

    for( int i=0; i < (size/SETSIZE) ; i++, idx+=SETSIZE)
    {
        bitset<SETSIZE> set(binary_str.substr(idx, idx+SETSIZE));
        hex_str << set.to_ulong();
    }

    hex_str << dec << endl;
}


/**
* On declare abitrairement que toute mesure superieure a 20 vaut '1' sinon elle vaut '0'
**/
static void extract_data(vector<unsigned int> data)
{

//    cout<<"extract data"<<endl;

    unsigned int seuil = 3;
    //on parcourt le message
    unsigned int i = 0;
    unsigned int frontprecedant = 0;
    unsigned int frontactuel = 0;
    unsigned int nbfront = 0;
    unsigned int frontinitial = 0;
    bool synchrodetecte =true;
    while(i<data.size()-40)
    {
        //on detecte un front montant valide cad on est a '0' et 20 '1' suivent
        bool front = true;
        if(data[i]<seuil)
        {

            for(unsigned int k=0; k<20; k++)
            {
                if(data[i+k]>seuil)
                {
                    front = front && true;
                }
                else
                {
                    front = front && false;
                }
            }
        }
        if(front)
        {
            frontactuel = i;
            //on est dans sur le premier front
            if(frontprecedant==0)
            {
                frontprecedant = frontactuel;
                frontinitial = frontactuel;
            }


            //si l'ecart entre 2 front est inferieur a 58
            if(frontactuel-frontprecedant<60)
            {

                frontprecedant = frontactuel;
                nbfront++;

                if(nbfront==8)
                {
                    synchrodetecte = true;
                    break;
                }
            }
            else
            {
                frontprecedant=0;
            }
            i=i+40;
        }
        else
        {
            i++;
        }
    }

    if(synchrodetecte)
    {
        string messbinaire;
        //cout<<"indice front synchro initial:"<<frontinitial<<" indice front synchro final:"<<frontactuel<<endl;
        //synchro detecte 10101010101010
        unsigned int dureebit=floor((frontactuel-frontinitial)/14)-1;
        cout<<"nb points d'un bit :"<<dureebit<<endl;
        dureebit = 28;
//        if(dureebit >20){
          //le signal commence deux bits plus loin apres '10' final de synchro
            unsigned int indicedebut = frontactuel+2*dureebit;
            unsigned int indicechangement = indicedebut;
            unsigned int indice = indicedebut;
            bool etathaut = false;
            while(indice<data.size())
            {
                //on detecte un front montant
                if(!etathaut&&data[indice]>seuil)
                {
                    etathaut = true;
                    float nbbitsbas = ceil((indice-indicechangement)/dureebit);
                    for(unsigned int k = 0; k<nbbitsbas; k++)
                    {
                        messbinaire+='0';
                    }

                    //cout<<"Etat haut :"<<indice<<"(indice-indicechangement)" <<indice-indicechangement<<"  nb bit bas:"<< nbbitsbas<<endl;
                    indicechangement = indice;
                }

                //on detecte un front descendant
                if(etathaut&&data[indice]<seuil)
                {
                    etathaut = false;
                    float nbbitshaut = ceil((indice-indicechangement)/dureebit);
                    for(unsigned int k = 0; k<nbbitshaut; k++)
                    {
                        messbinaire+='1';
                    }
                    //cout<<"Etat bas :"<<indice<<" nb bit haut:"<< nbbitshaut<<endl;
                    indicechangement = indice;

                }

                indice=indice+1;
            }



            //cout<<messbinaire<<endl;
            ostringstream hex_str;
            to_hex_str(messbinaire, hex_str);
            if(hex_str.str().substr(0, enteteThermometre.size()) == enteteThermometre)
            {
                string dataTempe =  hex_str.str().substr(enteteThermometre.size()).substr(0,3);
                double tempe = atof(dataTempe.c_str());
                cout << "Température: " <<(tempe-400)/10<<"°C"<< endl;
            } else if(hex_str.str().substr(0, enteteHumidite.size()) == enteteHumidite)
            {
                string dataHumi = hex_str.str().substr(enteteHumidite.size()).substr(1,2);
                cout <<"Humidité: "<< dataHumi<<" %"<< endl;

            } else {
                cout<<hex_str.str()<<endl;
            }
//        }

    }

}


static void decodeWS1501(unsigned char *buf, uint32_t len)
{

    //parcourt du buffer
    float datasize = len/2;
    vector<unsigned int> data;
    data.reserve(int(datasize));

    int i = 0;
    int q = 0;
    for (int k=0; k<(int)len-1; k=k+2)
    {
        i = (int) buf[k]-128;
        q = (int) buf[k+1]-128;
        data.push_back(sqrt(i*i+q*q));
    }

    /*
     on extrait les trains de bits:
        si on a une moyenne de 15 sur 500 points c'est que l'on est dans la synchro
        la data a un longeur totale inferieure a 4500 points
    */
    unsigned int moyenne_seuil_synchro = 3;
    unsigned int nb_ech_synchro = 500;
    unsigned int nb_ech_message = 4500;

    unsigned l = 0;
    unsigned int max_moyenne = 0;
    while(l<datasize-nb_ech_message){
        unsigned int somme = 0;
        for(unsigned int k=0; k<nb_ech_synchro; k++)
        {
            somme += data[l+k];
        }
         unsigned int moyenne = somme/nb_ech_synchro;
        if(moyenne>max_moyenne){
            max_moyenne = moyenne;
        }


        if(moyenne>=moyenne_seuil_synchro)
        {



            vector<unsigned int>::const_iterator first = data.begin() + l;
            vector<unsigned int>::const_iterator last = data.begin() + l + nb_ech_message;
            vector<unsigned int> subdata(first, last);

            extract_data(subdata);
            l = l+nb_ech_message;
        }
        else
        {
            l = l+50;
        }
    }


    delete[] memblock;



}

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
	if (ctx) {
		if (do_exit)
			return;

		if ((bytes_to_read > 0) && (bytes_to_read < len)) {
			len = bytes_to_read;
			do_exit = 1;
			rtlsdr_cancel_async(dev);
		}

//		if (fwrite(buf, 1, len, (FILE*)ctx) != len) {
//			fprintf(stderr, "Short write, samples lost, exiting!\n");
//			rtlsdr_cancel_async(dev);
//		}

        decodeWS1501(buf, len);

		if (bytes_to_read > 0)
			bytes_to_read -= len;
	}
}

int main()
{

    struct sigaction sigact;

    int r =0;
    uint32_t frequency = 868428000;
    int gain = (int)(49.6 * 10);
    uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
    uint8_t *buffer;
    uint32_t out_block_size = DEFAULT_BUF_LENGTH;
    FILE * file;
    buffer = (uint8_t *)malloc(out_block_size * sizeof(uint8_t));

	r = rtlsdr_open(&dev, 0);
	if (r < 0) {
		fprintf(stderr, "Failed to open rtlsdr device.\n");
		exit(1);
	}


	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);

	/* Set the sample rate */
	rtlsdr_set_sample_rate(dev, samp_rate);

	/* Set the frequency */
    rtlsdr_set_center_freq(dev, frequency);

    /* Set the gain*/
	rtlsdr_set_tuner_gain_mode(dev, 1);
    rtlsdr_set_tuner_gain(dev, gain);

	/* Reset endpoint before we start reading from it (mandatory) */
    rtlsdr_reset_buffer(dev);


    file = fopen("/tmp/data.rtl", "wb");

    /* Async mode*/
    fprintf(stderr, "Reading samples in async mode...\n");
    //on ne sauvegarde pas dans un fichier d'ou (void *)0 sinon (void *)file
    r = rtlsdr_read_async(dev, rtlsdr_callback, (void *)file,
				      DEFAULT_ASYNC_BUF_NUMBER, out_block_size);

    fclose(file);
	rtlsdr_close(dev);
	free (buffer);

    return 0;
}
