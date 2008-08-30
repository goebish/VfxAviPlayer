#include <windows.h>
#include "gif.h"

// pour les messages d'erreurs
char eGif[200];

//==============================================================================
// classe Gif
//==============================================================================

//==============================================================================
// Constructeur
Gif::Gif(char *name) : image(0)
{
palette=new uchar[768];
uchar* fichier=0;			// pointeur sur les données du fichier
uchar* cFichier;	 	// pointeur courant sur les données du fichier
try {
	// ouvre le fichier et lit l'entête
	fichier = Ouvrir(name);
   cFichier = fichier+13;
   // Lit la palette globale si existe
	if ( isPaletteGlobale )
		cFichier=LirePalette(palette,cFichier);
   // cherche le bloc descripteur d'image
   cFichier=InfoBlock(cFichier);
   // Lit la palette locale si existe
   if ( isPaletteLocale )
   	cFichier=LirePalette(palette,cFichier);
   // création de l'image
   image = new uchar[imageW*imageH];
   // décodage
   DecodeLZW(cFichier);
   if ( fichier )
   	delete fichier;
   }
catch ( char* err )
	{
   if ( fichier )
   	delete fichier;
   if ( image )
   	delete image;
   if ( palette )
   	delete palette;
   strcpy(eGif,err);
   strcat(eGif," : ");
   strcat(eGif,name);
   throw(eGif);
   }
}

//==============================================================================
// Destructeur
Gif::~Gif()
{
if ( image )
	delete image;
if ( palette )
	delete palette;
}

//==============================================================================
// Ouvre le fichier et test si la version est correcte
// Charge également les informations du bloc descripteur d'écran
// renvoi un pointeur sur les données du fichier
uchar* Gif::Ouvrir(char* name)
{
	HANDLE hFile;
			// ouvre le fichier en lecture
	hFile=CreateFile(name,GENERIC_READ,FILE_SHARE_READ,
								NULL,OPEN_EXISTING,0,0);
	if ( hFile==INVALID_HANDLE_VALUE )
	   throw("Impossible de lire le fichier");

	   // trouve la taille du fichier
	int taille=GetFileSize(hFile,NULL);
	if ( taille==-1 )
		throw("Impossible de trouver la taille du fichier");
	uchar* fichier = new uchar[taille];

		// lecture du fichier
	DWORD numByteRead;
	if ( !ReadFile(hFile,fichier,taille,&numByteRead,NULL) )
		throw("Erreur de lecture");

	   // ferme le fichier
	CloseHandle(hFile);

		// vérification "GIF" et "89a"
	if ( fichier[0]!='G' || fichier[1]!='I' || fichier[2]!='F' ||
		  fichier[3]!='8' || fichier[4]!='9' || fichier[5]!='a' )
		throw("Image gif non valide");

	   // trouve le nombre de couleur de l'image
	nbCouleur=1<<(((fichier[10]&0x70)>>4)+1);

		// lecture du bloc descripteur d'écran
	ecranW=fichier[6]+(fichier[7]<<8);
	ecranH=fichier[8]+(fichier[9]<<8);
	isPaletteGlobale=fichier[10]&0x80;
	coulFond=fichier[11];
	return fichier;
}

//==============================================================================
// Lit la palette de couleur
uchar* Gif::LirePalette(uchar* palette,uchar* cFichier)
{
memcpy(palette,cFichier,nbCouleur*3);
return cFichier+nbCouleur*3;
}

//==============================================================================
// Lit le block descripteur d'image
uchar* Gif::InfoBlock(uchar* cFichier)
{
   // saute les blocs d'extensions
while ( cFichier[0]==0x21 )
	// saute le bloc : taille du bloc = cFichier[2]+1
   cFichier+=4+cFichier[2];

if ( cFichier[0]!=0x2C )
	throw("Impossible de trouver le bloc descripteur d'image");

   // lecture du bloc descripteur d'image
imageW=cFichier[5]+(cFichier[6]<<8);
imageH=cFichier[7]+(cFichier[8]<<8);
isPaletteLocale=cFichier[10]&0x80;
return cFichier+10;		// taille du bloc = 10
}

//==============================================================================
// Decode l'image
void Gif::DecodeLZW(uchar* cFichier)
{
	// lit la taille du code
int tailleCode=cFichier[0];
cFichier++;

	// Initialisation pour le decodage LZW
int codeClear=1<<tailleCode;
int codeFin=1<<tailleCode;
int abSuffx[4096];		// alphabet suffixe et prefixe
int abPrefx[4096];		// prefixe codé, suffixe concret
int abPos=codeFin+1;	  	// position des nouvelles chaînes dans l'alphabet
uchar pileDecode[1281];	// pile de decodage
int posPileDec=0;			// position dans la pile de decodage
int tempPosPileDec;		// sauvegarde temporaire de la position de la pile
int nbBit=tailleCode+1;	// nombre de bit de l'alphabet
int maxCode=1<<nbBit;	// code maxi pour le nombre de bits courant
int curCode;				// code courant
int oldCode=-1;			// ancien code
int tempCode;				// sauvegarde temporaire
int restBit=0;				// bits restant dans le dernier octet lu
int lByte=0;				// dernier octet lu
int maskBit=0xFFFFFFFF>>(32-nbBit);		// mask pour retrouver le code courant
bool isCasSpecial=false;	// indique un cas special

// Initialisation pour le chargement des données par bloc
int nbBuff=0;					// nombre d'éléments restant dans un bloc

// autre variables
bool notFinie=true;			// indique la fin de la décompression
int posImage=0;				// pointeur dans l'image

// boucle de décodage
while ( notFinie )
	{
		// lit le code courant
   curCode = lByte;
   while ( restBit<nbBit )		// tant qu'on n'a pas lu le nombre de bit correcte
   	{
         	// vérifie qu'il reste des données dans le buffer
	   if ( nbBuff<=0 )
   		{
   		nbBuff=cFichier[0];
	      cFichier++;
   	   }

      		// lit un octet du buffer décalé du nombre de bit restant dans lByte
      		// et incrémentation du pointeur
	   curCode |= cFichier[0]<<restBit;
      cFichier++;
      nbBuff--;					// un de moins
      restBit+=8;					// 8 bit de plus de mit dans curCode
		}
      // nb de bit restant dans le dernier octet utilisé
   restBit-=nbBit;
   	// lit le dernier octet et enleve les bits lu
   lByte=curCode>>nbBit;
   curCode&=maskBit;

   if ( curCode==codeFin || curCode==codeClear )
   	{
      if ( curCode==codeClear )
      	{		// code re-initialisation de l'alphabet
      	nbBit=tailleCode+1;
         maxCode=1<<nbBit;
         maskBit=0xFFFFFFFF>>(32-nbBit);
         abPos=codeFin+1;
         oldCode=-1;
      	}
      if ( curCode==codeFin )
      	{		// code fin d'image
      	notFinie=false;
         }
      }
   else
   	{ // code quelconque
      tempCode=curCode;		// sauvegarde temporaire
      	// test si le code est valide
      if ( curCode>abPos )
      	throw("Erreur de décompression");
         // test si cas spécial
      if ( curCode==abPos )
      	{
         isCasSpecial=true;
         curCode=oldCode;
         }
      while ( curCode>codeFin )
        	{		// decode le code
			pileDecode[posPileDec++]=(uchar)abSuffx[curCode];	// lit le char decodé
         curCode=abPrefx[curCode];						// lit le code suivant
         }
      tempPosPileDec=posPileDec;
        	// reste à mettre le code concret
      pileDecode[posPileDec++]=(uchar)curCode;
        	// Maintenant, on deplie la pile pour la mettre dans l'image
      while ( posPileDec!=0 )
         image[posImage++]=pileDecode[--posPileDec];
         // test si on doit ajouter une entrée dans l'alphabet
      if ( isCasSpecial )
        	{
         isCasSpecial=false;		// reset du cas special
         	// ajoute à l'image le premier char de la chaîne
			image[posImage++]=pileDecode[tempPosPileDec];
         }
      if ( oldCode!=-1 )
        	{
           	// on prend pour suffx le premier char de la chaîne
            // => le dernier de la pile de decodage
      	abSuffx[abPos]=pileDecode[tempPosPileDec];
         abPrefx[abPos++]=oldCode;	  	// on le concaténe au code précedent
           	// test si on déborde le nombre de bits
         if ( abPos==maxCode )
           	{
            nbBit++;			// augmente le nombre de bits
            maxCode=1<<nbBit;		// met à jour maxCode et maskBit
            maskBit=0xFFFFFFFF>>(32-nbBit);
            }
         }
      oldCode=tempCode;		// met à jour oldCode
      }		// fin du if curCode==codeClear || curCode==codeFin
   }		// fin du while
}

//==============================================================================
// Convertie l'image 8 bits en 16 bits : 5-6-5
void Gif::Convert8to16(short int* dest)
{		// commence par convertir la palette
	short int tempPal[768];			// palette temporaire 16 bits
	for (int i=0; i<nbCouleur; i++)
		tempPal[i]=(short int)(
   				  ((palette[i*3]>>3)<<11) |		// rouge
   				  ((palette[i*3+1]>>2)<<5) |		// vert
				  (palette[i*3+2]>>3));				// bleu
		// converti l'image
	for (int i=0; i<imageW*imageH; i++)
		dest[i]=tempPal[image[i]];
}

//==============================================================================
// Convertie l'image 8 bits en 16 bits : 5-5-5
void Gif::Convert8to15(short int* dest)
{		// commence par convertir la palette
	short int tempPal[768];			// palette temporaire 16 bits
	for (int i=0; i<nbCouleur; i++)
		tempPal[i]=(short int)(
   				  ((palette[i*3]>>3)<<10) |		// rouge
   				  ((palette[i*3+1]>>3)<<5) |		// vert
				  (palette[i*3+2]>>3));				// bleu
		// converti l'image
	for (int i=0; i<imageW*imageH; i++)
		dest[i]=tempPal[image[i]];
}

//==============================================================================
// Convertie l'image 8 bits en 24 bits
void Gif::Convert8to24(uchar* dest)
{
	for (int i=0; i<imageW*imageH; i++)
	{
	   dest[i*3]=palette[image[i]*3];
	   dest[i*3+1]=palette[image[i]*3+1];
	   dest[i*3+2]=palette[image[i]*3+2];
   }
}

//==============================================================================
// Convertie l'image 8 bits en 32 bits
void Gif::Convert8to32(int* dest)
{
	int tempPal[768];		// palette temporaire 32 bits
	for (int i=0; i<nbCouleur; i++)
		tempPal[i]=(palette[i*3]<<16) |		// rouge
   				  (palette[i*3+1]<<8) |		// vert
				  palette[i*3+2];				// bleu
	   // converti l'image
	for (int i=0; i<imageW*imageH; i++)
		dest[i]=tempPal[image[i]];
}
