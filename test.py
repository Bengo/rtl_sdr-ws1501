import numpy
import matplotlib.pyplot as plt
import math


"""
bengo@bengo-System:~/Outils/SDR/data/test/decode$ rtl_sdr -f 868428000 -s 250000 data.raw -g 49.6 -S
Found 1 device(s):
  0:  Realtek, RTL2838UHIDIR, SN: 00000001

Using device 0: Generic RTL2832U OEM
Found Rafael Micro R820T tuner
Exact sample rate is: 250000.000414 Hz
Tuned to 868428000 Hz.
Tuner gain set to 49.600000 dB.
Reading samples in sync mode...
^CSignal caught, exiting!

User cancel, exiting...
bengo@bengo-System:~/Outils/SDR/data/test/decode$ python test.py aaaa2dd4a6b504311091200030fe41feac
Temperature : 3.1 deg
Pourcentage humidite: 91
aaaa2dd4a6b504311091200030fe41feac
Temperature : 3.1 deg
Pourcentage humidite: 91
aaaa2dd4a6b504311091200030fe41feac
aaaa2dd4a6930486200030fedd
aaaa2dd4a692200030fe13
aaaa2dd4a692200030fe13
aaaa2dd4a693200030fe41fe4a




"""
def main():
 fd = open('/tmp/data.rtl', 'rb');

 data_raw_IQ = numpy.fromfile(file=fd, dtype=numpy.uint8);
 fd.close();
 offset = 128;


 data_IQ = numpy.array(data_raw_IQ,numpy.int16)-offset;
 #data = numpy.absolute(numpy.array(data_raw_IQ,numpy.int16)-offset);
 data = [];
 #on garde l'amplitude des I/Q sqrt(IxI+QxQ)
 for i in range(0,int(len(data_IQ)-1),2):
  data.append(math.sqrt(data_IQ[i]*data_IQ[i]+data_IQ[i+1]*data_IQ[i+1]))

 #numpy.array(data,numpy.int8).tofile("dataR.raw", sep="", format="%i")
 
 plt.plot(data)
 plt.show()
 #print len(data)
 #on extrait les trains de bits
 #la synchro a environ une moyenne de  30 sur 500 points
 #on parcourt 50 points par 50 points
 moyenne_synchro = 50
 taille_synchro = 500
 taille_data = 4500
 taille_parcourt = 100
 i = 0
 while i <  len(data)-taille_data:
   if(numpy.mean(data[i:i+taille_synchro])>moyenne_synchro):
    print "debut de trame a %f seconde" %float(i/250000)
    print "moyenne %f" %numpy.mean(data[i:i+taille_synchro])
    extract_data(data[i:i+taille_data])
    i+=taille_data
   else:
    i+=taille_parcourt

def extract_data(data):
 #on lisse la courbe par moyenne glissante n fois
 mobile_size = 3;
 ordre_moyenne = 12;

 data_lissee = numpy.array(data[0:len(data)-mobile_size]);
 """
 for j in range(0,ordre_moyenne):
  #on parcourt la liste
  for i in range(0,len(data_lissee)):	
   elem=numpy.mean(data_lissee[i:i+mobile_size]);
   data_lissee[i]=elem;
"""
 plt.plot(data_lissee)
 plt.show()
 seuil = 70;

 for i in range(0,len(data_lissee)):
  if(data_lissee[i]<seuil):
   data_lissee[i]=0;
  else:
   data_lissee[i]=1;

 #plt.plot(data_lissee)
 #plt.show()
 
 #on parcourt les bits de synchro
 #on a 16 bits '1010101010101010' de synchroniqation
 #ils permettent de calculer la duree d'un bit
 indice_montant_precedant = 0
 indice_montant = 0;
 nb_bit_haut_synchro_detecte=0;
 nb_bit_haut_synchro=7;
 duree_2bits=[];
 hits_bit = 0;
 debut_trame = 0;
 data=[1,0]
 for i in range(0,len(data_lissee)-20):
  #on detecte un front montant valide cad on est a '0' et 20 '1' suivent
  if data_lissee[i] == 0 and data_lissee[i+1:i+20].all() == 1 :
   if(indice_montant!=0):
    indice_montant_precedant=indice_montant;
   else :
    debut_trame = i;
   indice_montant = i+1;	
		
   if(indice_montant_precedant != 0):
    if((indice_montant-indice_montant_precedant)<500):			
     if(nb_bit_haut_synchro_detecte<nb_bit_haut_synchro):
      duree_2bits.append(indice_montant-indice_montant_precedant)
      data.append(1);
      data.append(0);
      nb_bit_haut_synchro_detecte += 1;
     else:
     #print duree_2bits
      hits_bit = round(numpy.mean(duree_2bits)/2);
      print "nb points d'un bit %i" %hits_bit
      break;
    else :
     debut_trame = i;
   else :
    indice_montant_precedant = 0;
			
 #on parcourt le message
 #print "indice debut trame: %i" %int(debut_trame)

 etat_haut=False;
 if(indice_montant!=0 and hits_bit>0):
  debut_message=debut_trame+len(data)*hits_bit;
  indice_changement_etat=debut_message;
  #print debut_message
  for i in range(int(debut_message),len(data_lissee)-1):
   #on detecte un front montant
   if(etat_haut != True and data_lissee[i]==0 and data_lissee[i+1]==1) :

    nb_bits_zero = round((i - indice_changement_etat)/hits_bit);
    for j in range(0,int(nb_bits_zero)):
     data.append(0);
     etat_haut = True;
     indice_changement_etat = i+1;
   #on detecte un front descendant
   elif(etat_haut == True and data_lissee[i]==1 and data_lissee[i+1]==0) :
    nb_bits_un = round((i - indice_changement_etat)/hits_bit);
    for j in range(0,int(nb_bits_un)):
     data.append(1);
    etat_haut = False;
    indice_changement_etat = i+1;
 #print data;

 #completer avec des 0 pour obtenir un nombre de bit multiple de 4
 while(len(data)%4 != 0):
  data.append(0);

 #print len(data)
 data_hexa=[]
 #print data
 for i in range(0,int(len(data)),4) :
  hexa_elts=[str(hexa) for hexa in data[i:i+4]]
  hexa_string = "".join(hexa_elts)
  #for j in range(0,4):
   #hexa_string+=str(data[i+j]);
  data_hexa.append(hex(int(hexa_string,2))[2:]);

 hexalist = [elt for elt in data_hexa]
 hexa = "".join(hexalist)

 print("data %s",hexa);

 if( len(data_hexa)>=21) :
  #print data_hexa[13:17]
  try:
   temp_list = [elt for elt in data_hexa[12:16]]
   value_temp = "".join(temp_list)
   temp = (float(value_temp)-400)/10
   print("Temperature : %.1f deg",temp)
  except ValueError:
   print("Oops!  Temperature was no valid number: %s",value_temp)
  try: 
   hygro_list = [elt for elt in data_hexa[16:20]]
   value_hygro = "".join(hygro_list)
   hygro = (float(value_hygro)-1000)
   print("Pourcentage humidite: %.0f",hygro)
  except ValueError:
   print("Oops!  humidity was no valid number: %s",value_hygro)
 
if __name__=="__main__":
 main()

