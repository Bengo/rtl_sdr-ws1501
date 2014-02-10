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
#include <map>
#include <numeric>

#define MOYENNE_SYNCHRO 25
#define SEUIL_HAUT 50
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
string synchroAnemo = "20003";

bool isTempFound = false;
bool isHumidityFound = false;
bool isWindFound = false;
vector<float> temperatures;
vector<unsigned long> humidites;
vector<float> vitessesVent;
vector<char> directionsVent;

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

    for( unsigned int i=0; i < (size/SETSIZE) ; i++, idx+=SETSIZE)
    {
        bitset<SETSIZE> set(binary_str.substr(idx, idx+SETSIZE));
        hex_str << set.to_ulong();
    }

    hex_str << dec << endl;
}


/**
* On declare abitrairement que toute mesure superieure a SEUIL_HAUT vaut '1' sinon elle vaut '0'
**/
static void extract_data(vector<unsigned int> data)
{

//    cout<<"extract data"<<endl;

    unsigned int seuil = SEUIL_HAUT;
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
        //cout<<"nb points d'un bit :"<<dureebit<<endl;
        //le signal commence deux bits plus loin apres '10' final de synchro
        unsigned int indicedebut = frontactuel+2*dureebit;
        unsigned int indicechangement = indicedebut;
        unsigned int indice = indicedebut;
        bool etathaut = false;
        while(indice<data.size())
        {
//            unsigned int index_moyenne = 0;
//            unsigned int moyenne = 0;
//            for(index_moyenne=0;index_moyenne<dureebit;index_moyenne++){
//                moyenne += data[index_moyenne];
//            }
//            moyenne = floor(moyenne/dureebit)

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




        ostringstream hex_str;
        to_hex_str(messbinaire, hex_str);

//        cout<<messbinaire<<endl;
//        cout<<hex_str.str()<<endl;
        if(hex_str.str().substr(0, enteteThermometre.size()) == enteteThermometre)
        {
            string dataTempe =  hex_str.str().substr(enteteThermometre.size()).substr(0,3);
            double tempe = atof(dataTempe.c_str());

            cout << "Température: " <<(tempe-400)/10<<"°C"<< endl;
            temperatures.push_back((tempe-400)/10);
            isTempFound = true;

        }
        else if(hex_str.str().substr(0, enteteHumidite.size()) == enteteHumidite)
        {
            string dataHumi = hex_str.str().substr(enteteHumidite.size()).substr(1,2);

            cout <<"Humidité: "<< dataHumi<<" %"<< endl;
            isHumidityFound= true;
            humidites.push_back(atof(dataHumi.c_str()));
        }

        //detection vent direction et intensite
        if(hex_str.str().find(synchroAnemo) != string::npos)
        {
            size_t debutSynchro = hex_str.str().find(synchroAnemo);
            //direction
            size_t indiceDirection = debutSynchro + synchroAnemo.size();
//            string direction = directions[hex_str.str().at(indiceDirection)];
//            cout<<direction<<endl;
            //intensite
            if(hex_str.str().size()>indiceDirection+2)
            {
                string intensiteHexa = hex_str.str().substr(indiceDirection+1,2);
                unsigned long intensite;
                std::stringstream ss;
                ss << std::hex << intensiteHexa;
                ss >> intensite;
                std::cout << "Vent: "<< intensite*0.36 << "  "<< hex_str.str().at(indiceDirection) <<endl;
                isWindFound = true;
                vitessesVent.push_back(intensite*0.36);
                directionsVent.push_back(hex_str.str().at(indiceDirection));
            }
        }

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
    unsigned int moyenne_seuil_synchro = MOYENNE_SYNCHRO;
    unsigned int nb_ech_synchro = 500;
    unsigned int nb_ech_message = 4500;

    unsigned l = 0;
    unsigned int max_moyenne = 0;
    while(l<datasize-nb_ech_message)
    {
        unsigned int somme = 0;
        for(unsigned int k=0; k<nb_ech_synchro; k++)
        {
            somme += data[l+k];
        }
        unsigned int moyenne = somme/nb_ech_synchro;
        if(moyenne>max_moyenne)
        {
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
    static map<char, string> directions;
    directions['0'] = "N";
    directions['1'] = "NNE";
    directions['2'] = "NE";
    directions['3'] = "ENE";
    directions['4'] = "E";
    directions['5'] = "ESE";
    directions['6'] = "SE";
    directions['7'] = "SSE";
    directions['8'] = "S";
    directions['9'] = "SSW";
    directions['a'] = "SW";
    directions['b'] = "WSW";
    directions['c'] = "W";
    directions['d'] = "WNW";
    directions['e'] = "NW";
    directions['f'] = "NNW";


    if (ctx)
    {
        if (do_exit)
            return;


        if ((bytes_to_read > 0) && (bytes_to_read < len))
        {
            len = bytes_to_read;
            do_exit = 1;
            rtlsdr_cancel_async(dev);
        }

        if (fwrite(buf, 1, len, (FILE*)ctx) != len)
        {
            fprintf(stderr, "Short write, samples lost, exiting!\n");
            rtlsdr_cancel_async(dev);
        }

        decodeWS1501(buf, len);

        if (bytes_to_read > 0)
            bytes_to_read -= len;

        if(isHumidityFound && isWindFound && isTempFound)
        {
            do_exit = 1;
            rtlsdr_cancel_async(dev);
            //Temperature moyenne
            float tempeMoyenne = std::accumulate(temperatures.begin(), temperatures.end(), 0.0 )/ temperatures.size();
            unsigned long humiditeMoyenne = accumulate(humidites.begin(), humidites.end(), 0 )/ humidites.size();
            char directionMoyenne = accumulate(directionsVent.begin(), directionsVent.end(), 0 )/ directionsVent.size();
            float ventMoyen = accumulate(vitessesVent.begin(), vitessesVent.end(), 0.0 )/ vitessesVent.size();
            cout<<"   Mesures realisees   "<<endl;
            cout<<"-----------------------"<<endl;
            cout<<"Vent :"<<ventMoyen<<" km/h "<<directions[directionMoyenne]<<endl;
            cout<<"Temperature :"<<tempeMoyenne<<" °C"<<endl;
            cout<<"Humidite : "<<humiditeMoyenne<<"%"<<endl;

        }
    }
}

int main()
{
    struct sigaction sigact;

    int r =0;
    uint32_t frequency = 868428000;
    //int gain = (int)(49.6 * 10);
    uint32_t samp_rate = DEFAULT_SAMPLE_RATE;
    uint8_t *buffer;
    uint32_t out_block_size = DEFAULT_BUF_LENGTH;
    FILE * file;
    buffer = (uint8_t *)malloc(out_block_size * sizeof(uint8_t));

    r = rtlsdr_open(&dev, 0);
    if (r < 0)
    {
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
    r = rtlsdr_set_center_freq(dev, frequency);
    if (r < 0)
        fprintf(stderr, "WARNING: Failed to set center freq.\n");
    else
        fprintf(stderr, "Tuned to %u Hz.\n", rtlsdr_get_center_freq(dev));
    /* Set the gain*/
    rtlsdr_set_tuner_gain_mode(dev, 0);

//    r = rtlsdr_set_tuner_gain(dev, gain);
//        if (r < 0)
//            fprintf(stderr, "WARNING: Failed to set tuner gain.\n");
//        else
//            fprintf(stderr, "Tuner gain set to %f dB.\n", gain/10.0);

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
