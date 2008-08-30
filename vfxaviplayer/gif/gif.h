/*******************************************************************************
									classe Gif
							Auteur : Tanguy Lachenal
                     Version : 2.1

Cette classe permet de charger une image GIF dans un tableau dont la couleur
d�pend de la palette.
Elle contient �galement des m�thodes pour convertir ce tableau dans un format
16 bits (5-6-5 ou 5-5-5), 24 bits et 32 bits.
*******************************************************************************/

#if !defined(_GIF_H_)
#define _GIF_H_

#include <windows.h>

// d�finition du type uchar
typedef unsigned char uchar;

//==============================================================================
// Classe de base pour les images gifs
class Gif
{
public:
	Gif(char*);
   ~Gif();

// pour convertir l'image dans un format sp�cifique.
   void Convert8to16(short int*);	// 5-6-5
   void Convert8to15(short int*);	// 5-5-5
   void Convert8to24(uchar*);
   void Convert8to32(int*);

	int coulFond;					// couleur de fond
	int imageW,imageH;			// largeur et hauteur de l'image

   uchar* image;					// pointeur sur l'image d�cod�
   uchar* palette;				// pointeur sur la palette de couleur
   int nbCouleur;					// nombre de couleur de l'image

private:
   uchar* Ouvrir(char*);
   uchar* LirePalette(uchar*,uchar*);
   uchar* InfoBlock(uchar*);
   void DecodeLZW(uchar*);

	bool isPaletteLocale;		// indique la pr�sence d'une palette locale
	bool isPaletteGlobale;		// pr�sence d'une palette globale
	int ecranW,ecranH;			// largeur et hauteur de l'�cran
};

#endif
