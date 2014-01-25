#include<iostream>
#include<fstream>
#include<vector>
#include<math.h>
#include <string>
#include <bitset>
#include <sstream>
#include <stdlib.h>

#define SETSIZE 4
using namespace std;

ifstream::pos_type size;
unsigned char* memblock;

string enteteThermometre = "2dd4a1430";
string enteteHumidite = "2dd4a1431";

void to_hex_str(string& binary_str, ostringstream& hex_str)
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
void extract_data(vector<unsigned int> data)
{

    unsigned int seuil = 20;
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
        //cout<<hex_str.str()<<endl;

        if(hex_str.str().substr(0, enteteThermometre.size()) == enteteThermometre)
        {
            string dataTempe =  hex_str.str().substr(enteteThermometre.size()).substr(0,3);
            double tempe = atof(dataTempe.c_str());
            cout << "Température: " <<(tempe-400)/10<<"°C"<< endl;
        }

        if(hex_str.str().substr(0, enteteHumidite.size()) == enteteHumidite)
        {
            string dataHumi = hex_str.str().substr(enteteHumidite.size()).substr(1,2);
            cout <<"Humidité: "<< dataHumi<<" %"<< endl;

        }

    }

}



int main ()
{
    ifstream file ("/tmp/data.raw", ios::in|ios::binary|ios::ate);
    if (file.is_open())
    {
        size = file.tellg();
        memblock = new unsigned char[size];
        file.seekg (0, ios::beg);
        file.read ((char*)memblock, size);
        file.close();

        //on ne garde que l'amplitude des I/Q
        float datasize = size/2;
        vector<unsigned int> data;
        data.reserve(int(datasize));
        int i = 0;
        int q = 0;
        for (int k=0; k<(int)size-1; k=k+2)
        {
            i = (int) memblock[k]-128;
            q = (int) memblock[k+1]-128;
            data.push_back(sqrt(i*i+q*q));
        }

        /*
         on extrait les trains de bits:
            si on a une moyenne de 15 sur 500 points c'est que l'on est dans la synchro
            la data a un longeur totale inferieure a 4500 points
        */
        unsigned int moyenne_seuil_synchro = 15;
        unsigned int nb_ech_synchro = 500;
        unsigned int nb_ech_message = 4500;

        unsigned l = 0;
        while(l<datasize-nb_ech_message){
            unsigned int somme = 0;
            for(unsigned int k=0; k<nb_ech_synchro; k++)
            {
                somme += data[l+k];
            }
             unsigned int moyenne = somme/nb_ech_synchro;

            if(moyenne>moyenne_seuil_synchro)
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
    else cout << "Unable to open file"<<endl;
    return 0;
}
