/*
   Quick MUS to Midi converter.
   (C) 1995,96 Sebastien Bacquet  ( bacquet@iie.cnam.fr )

   Ported to unix by Hans Peter Verne ( hpv@kjemi.uio.no )

   This is free software, distributed under the terms of the
   GNU General Public License. For details see the file COPYING.

   Use gcc to compile, if possible.  Please look in  "qmus2mid.h"
   for system dependencies, in particular the int2 and int4 typedef's.

   To compile for MS-DOS, #define MSDOG or use -DMSDOG parameter

   Otherwise, vanilla unix is assumed, but it still compiles under dos.

   For the time being, this only works for little-endian machines,
   such as i86, dec-mips, alpha;  but not rs6000, sparc....

*/

/* Hacked for DOOM Win32 port by Petteri Kangaslampi <pekangas@sci.fi> */

//#define MSDOG

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_alloc.h"

int Printf (char *, ...);

/* [Petteri]  warning C4761: integral size mismatch in argument; conversion
   supplied */
#ifdef _MSC_VER
#   pragma warning(disable:4761)
#endif


#ifdef MSDOG
#include <io.h>
#include <dir.h>
//#include <pc.h>
#include <process.h>
//#include <keys.h>
#else
#include <sys/types.h>
#endif

#include <sys/stat.h>

#include "qmus2mid.h"

FILE *file_mus, *file_mid ;

int4 TRACKBUFFERSIZE = 65536L ;  /* 64 Ko */

#ifdef MSDOG
char tmp[MAXPATH] ;
#endif

size_t fwrite2(const int2 *ptr, size_t size, FILE *file)
{
  int4 rev = 0, work;
  int i;

  for( i = 0 ; (size_t)i < size ; i++ ) {
// VC++ didn't like this magic (but only for the release build!)
//    rev = (rev << 8) + (((*ptr) >> (i*8)) & 0xFF) ;
    rev <<= 8;
    work = (*ptr) >> (i << 3);
    rev |= (work & 0xFF);
  }

  return fwrite( &rev, size, 1, file ) ;
}


void FreeTracks( struct Track track[] )
{
  int i ;

  for( i = 0 ; i < 16 ; i++ )
    if( track[i].data )
      free( track[i].data ) ;
}


#ifdef MSDOG
void Close( void )
{
    fclose(file_mus);
    fclose(file_mid);
    unlink( tmp ) ;
}
#else

#define Close()

char LittleEndian(void)
{
  short i = 0x0001;
  char *p = (char *) &i;
  return *p;
}

#endif


void TWriteByte( char MIDItrack, char byte, struct Track track[] )
{
  int4 pos;

  pos = track[MIDItrack].current;
  if( pos < TRACKBUFFERSIZE )
    track[MIDItrack].data[pos] = byte;
  else
    {
      Printf("ERROR : Track buffer full.\n"
             "Increase the track buffer size (option -size).\n" );
      FreeTracks( track );
      Close() ;
      exit( EXIT_FAILURE );
    }
  track[MIDItrack].current++;
}


void TWriteVarLen( int tracknum, register int4 value,
                  struct Track track[] )
{
  register int4 buffer ;

  buffer = value & 0x7f ;
  while( (value >>= 7) )
    {
      buffer <<= 8 ;
      buffer |= 0x80 ;
      buffer += (value & 0x7f) ;
    }
  while( 1 )
    {
      TWriteByte( tracknum, buffer, track ) ;
      if( buffer & 0x80 )
        buffer >>= 8 ;
      else
        break;
    }
}


int ReadMUSheader( MUSheader *MUSh, FILE *file )
{
  if( fread( MUSh->ID, 4, 1, file ) != 1 ) return COMUSFILE ;
  if( strncmp( MUSh->ID, MUSMAGIC, 4 ) )
    return NOTMUSFILE ;
  if( fread( &(MUSh->ScoreLength),  2, 1, file ) != 1 ) return COMUSFILE ;
  if( fread( &(MUSh->ScoreStart),  2, 1, file ) != 1 ) return COMUSFILE ;
  if( fread( &(MUSh->channels),  2, 1, file ) != 1 ) return COMUSFILE ;
  if( fread( &(MUSh->SecChannels),  2, 1, file ) != 1 ) return COMUSFILE ;
  if( fread( &(MUSh->InstrCnt),  2, 1, file ) != 1 ) return COMUSFILE ;
  if( fread( &(MUSh->dummy),  2, 1, file ) != 1 ) return COMUSFILE ;
  MUSh->instruments = (int2 *) Calloc(MUSh->InstrCnt, sizeof(int2)) ;
  if( fread( MUSh->instruments, 2, MUSh->InstrCnt, file ) != MUSh->InstrCnt )
    {
      free( MUSh->instruments ) ;
      return COMUSFILE ;
    }
  free( MUSh->instruments ) ; /* suppress this line if you want to display
				 instruments later */
  return 0 ;
}


int WriteMIDheader( int2 ntrks, int2 division, FILE *file )
{
  fwrite( MIDIMAGIC , 10, 1, file ) ;
  fwrite2( &ntrks, 2, file) ;
  fwrite2( &division, 2, file ) ;
  return 0 ;
}

        /* maybe for ms-dog too ? */ /* Yes, why not ?... */
#define last(e)         ((unsigned char)(e & 0x80))
#define event_type(e)   ((unsigned char)((e & 0x7F) >> 4))
#define channel(e)      ((unsigned char)(e & 0x0F))

void TWriteString( char tracknum, const char *string, int length,
                   struct Track track[] )
{
  register int i ;

  for( i = 0 ; i < length ; i++ )
    TWriteByte( tracknum, string[i], track ) ;
}


void WriteTrack( int tracknum, FILE *file, struct Track track[] )
{
  int2 size ;
  size_t quot, rem ;

  /* Do we risk overflow here ? */
  size = (int2) track[tracknum].current+4 ;
  fwrite( "MTrk", 4, 1, file ) ;
  if( !tracknum ) size += 33 ;

  fwrite2( &size, 4, file ) ;
  if( !tracknum)
    fwrite( TRACKMAGIC1 "Quick MUS->MID ! by S.Bacquet", 33, 1, file ) ;
  quot = (size_t) (track[tracknum].current / 4096) ;
  rem = (size_t) (track[tracknum].current - quot*4096) ;
  fwrite( track[tracknum].data, 4096, quot, file ) ;
  fwrite( ((const unsigned char *) track[tracknum].data)+4096*quot, rem,
                         1, file ) ;
  fwrite( TRACKMAGIC2, 4, 1, file ) ;
}


void WriteFirstTrack( FILE *file )
{
  int2 size ;

  size = 43 ;
  fwrite( "MTrk", 4, 1, file ) ;
  fwrite2( &size, 4, file ) ;
  fwrite( TRACKMAGIC3 , 4, 1, file ) ;
  fwrite( "QMUS2MID (C) S.Bacquet", 22, 1, file ) ;
  fwrite( TRACKMAGIC4, 6, 1, file ) ;
  fwrite( TRACKMAGIC5, 7, 1, file ) ;
  fwrite( TRACKMAGIC6, 4, 1, file ) ;
}

int4 ReadTime( FILE *file )
{
  register int4 time = 0 ;
  int byte ;

  do
    {
      byte = getc( file ) ;
      if( byte != EOF ) time = (time << 7) + (byte & 0x7F) ;
    } while( (byte != EOF) && (byte & 0x80) ) ;

  return time ;
}

char FirstChannelAvailable( signed char MUS2MIDchannel[] )
{
  int i ;
  signed char old15 = MUS2MIDchannel[15], max = -1 ;

  MUS2MIDchannel[15] = -1 ;
  for( i = 0 ; i < 16 ; i++ )
    if( MUS2MIDchannel[i] > max ) max = MUS2MIDchannel[i] ;
  MUS2MIDchannel[15] = old15 ;

  return (max == 8 ? 10 : max+1) ;
}


int qmus2mid( const char *mus, const char *mid, int nodisplay,
             int2 division, int BufferSize, int nocomp )
{
  struct Track track[16] ;
  int2 TrackCnt = 0 ;
  unsigned char et, MUSchannel, MIDIchannel, MIDItrack, NewEvent ;
  int i, event, data, r ;
  static MUSheader MUSh ;
  int4 DeltaTime, TotalTime = 0, time, min, n = 0 ;
  unsigned char MUS2MIDcontrol[15] = {
    0,                          /* Program change - not a MIDI control change */
    0x00,                       /* Bank select */
    0x01,                       /* Modulation pot */
    0x07,                       /* Volume */
    0x0A,                       /* Pan pot */
    0x0B,                       /* Expression pot */
    0x5B,                       /* Reverb depth */
    0x5D,                       /* Chorus depth */
    0x40,                       /* Sustain pedal */
    0x43,                       /* Soft pedal */
    0x78,                       /* All sounds off */
    0x7B,                       /* All notes off */
    0x7E,                       /* Mono */
    0x7F,                       /* Poly */
    0x79                        /* Reset all controllers */
  }, MIDIchan2track[16] ;
  signed char MUS2MIDchannel[16] ;
#ifdef MSDOG
  char drive[MAXDRIVE], dir[MAXDIR], name[MAXFILE], ext[MAXEXT] ;
#endif
  char ouch = 0, sec ;
  struct stat file_data ;

  if( (file_mus = fopen( mus, "rb" )) == NULL )
    return COMUSFILE ;
  stat( mus, &file_data ) ;

#ifdef MSDOG
  fnsplit( mid, drive, dir, name, ext ) ;
  fnmerge( tmp, drive, dir, "tempmid", ".tmp" ) ;

  if( (file_mid = fopen( tmp, "wb" )) == NULL )
    {
      Close() ;
      return COTMPFILE ;
    }

#else

  /*  Why bother with a tmp-file anyway ? */
  /*  If I could have done differently...You know, DOS is DOS... */

  if( (file_mid = fopen( mid, "wb" )) == NULL )
    return CWMIDFILE ;
#endif

  r = ReadMUSheader( &MUSh, file_mus ) ;
  if( r )
    {
      Close() ;
      return r ;
    }
  if( fseek( file_mus, MUSh.ScoreStart, SEEK_SET ) )
    {
      Close() ;
      return MUSFILECOR ;
    }
  if( !nodisplay )
    Printf( "%s (%lu bytes) contains %d melodic channel%s.\n", mus,
           (unsigned long) file_data.st_size, MUSh.channels,
           MUSh.channels >= 2 ? "s" : "" );
  if( MUSh.channels > 15 )      /* <=> MUSchannels+drums > 16 */
    {
      Close() ;
      return TOOMCHAN ;
    }

  for( i = 0 ; i < 16 ; i++ )
    {
      MUS2MIDchannel[i] = -1 ;
      track[i].current = 0 ;
      track[i].vel = 64 ;
      track[i].DeltaTime = 0 ;
      track[i].LastEvent = 0 ;
      track[i].data = NULL ;
    }
  if( BufferSize )
    {
      TRACKBUFFERSIZE = ((int4) BufferSize) << 10 ;
      if( !nodisplay )
        Printf( "Track buffer size set to %d KB.\n", BufferSize ) ;
    }

  if( !nodisplay )
    {
      Printf( "Converting..." ) ;
      fflush( stdout ) ;
    }
  event = getc( file_mus ) ;
  et = event_type( event ) ;
  MUSchannel = channel( event ) ;
  while( (et != 6) && !feof( file_mus ) && (event != EOF) )
    {
      if( MUS2MIDchannel[MUSchannel] == -1 )
        {
          MIDIchannel = MUS2MIDchannel[MUSchannel ] =
            (MUSchannel == 15 ? 9 : FirstChannelAvailable( MUS2MIDchannel)) ;
          MIDItrack   = MIDIchan2track[MIDIchannel] = (unsigned char) (TrackCnt++);
          if( !(track[MIDItrack].data = (char *) malloc( TRACKBUFFERSIZE )) )
            {
              FreeTracks( track ) ;
              Close() ;
              return MEMALLOC ;
            }
        }
      else
        {
          MIDIchannel = MUS2MIDchannel[MUSchannel] ;
          MIDItrack   = MIDIchan2track [MIDIchannel] ;
        }
      TWriteVarLen( MIDItrack, track[MIDItrack].DeltaTime, track ) ;
      track[MIDItrack].DeltaTime = 0 ;
      switch( et )
        {
        case 0 :                /* release note */
          NewEvent = 0x90 | MIDIchannel ;
          if( (NewEvent != track[MIDItrack].LastEvent) || nocomp )
            {
              TWriteByte( MIDItrack, NewEvent, track ) ;
              track[MIDItrack].LastEvent = NewEvent ;
            }
          else
            n++ ;
          data = getc( file_mus ) ;
          TWriteByte( MIDItrack, data, track ) ;
          TWriteByte( MIDItrack, 0, track ) ;
          break ;
        case 1 :
          NewEvent = 0x90 | MIDIchannel ;
          if( (NewEvent != track[MIDItrack].LastEvent) || nocomp )
            {
              TWriteByte( MIDItrack, NewEvent, track ) ;
              track[MIDItrack].LastEvent = NewEvent ;
            }
          else
            n++ ;
          data = getc( file_mus ) ;
          TWriteByte( MIDItrack, data & 0x7F, track ) ;
          if( data & 0x80 )
            track[MIDItrack].vel = getc( file_mus ) ;
          TWriteByte( MIDItrack, track[MIDItrack].vel, track ) ;
          break ;
        case 2 :
          NewEvent = 0xE0 | MIDIchannel ;
          if( (NewEvent != track[MIDItrack].LastEvent) || nocomp )
            {
              TWriteByte( MIDItrack, NewEvent, track ) ;
              track[MIDItrack].LastEvent = NewEvent ;
            }
          else
            n++ ;
          data = getc( file_mus ) ;
          TWriteByte( MIDItrack, (data & 1) << 6, track ) ;
          TWriteByte( MIDItrack, data >> 1, track ) ;
          break ;
        case 3 :
          NewEvent = 0xB0 | MIDIchannel ;
          if( (NewEvent != track[MIDItrack].LastEvent) || nocomp )
            {
              TWriteByte( MIDItrack, NewEvent, track ) ;
              track[MIDItrack].LastEvent = NewEvent ;
            }
          else
            n++ ;
          data = getc( file_mus ) ;
          TWriteByte( MIDItrack, MUS2MIDcontrol[data], track ) ;
          if( data == 12 )
            TWriteByte( MIDItrack, MUSh.channels+1, track ) ;
          else
            TWriteByte( MIDItrack, 0, track ) ;
          break ;
        case 4 :
          data = getc( file_mus ) ;
          if( data )
            {
              NewEvent = 0xB0 | MIDIchannel ;
              if( (NewEvent != track[MIDItrack].LastEvent) || nocomp )
                {
                  TWriteByte( MIDItrack, NewEvent, track ) ;
                  track[MIDItrack].LastEvent = NewEvent ;
                }
              else
                n++ ;
              TWriteByte( MIDItrack, MUS2MIDcontrol[data], track ) ;
            }
          else
            {
              NewEvent = 0xC0 | MIDIchannel ;
              if( (NewEvent != track[MIDItrack].LastEvent) || nocomp )
                {
                  TWriteByte( MIDItrack, NewEvent, track ) ;
                  track[MIDItrack].LastEvent = NewEvent ;
                }
              else
                n++ ;
            }
          data = getc( file_mus ) ;
          TWriteByte( MIDItrack, data, track ) ;
          break ;
        case 5 :
        case 7 :
          FreeTracks( track ) ;
          Close() ;
          return MUSFILECOR ;
        default : break ;
        }
      if( last( event ) )
	{
          DeltaTime = ReadTime( file_mus ) ;
          TotalTime += DeltaTime ;
	  for( i = 0 ; i < (int) TrackCnt ; i++ )
	    track[i].DeltaTime += DeltaTime ;
        }
      event = getc( file_mus ) ;
      if( event != EOF )
                  {
          et = event_type( event ) ;
          MUSchannel = channel( event ) ;
        }
      else
        ouch = 1 ;
    }
  if( !nodisplay ) Printf( "done !\n" ) ;
  if( ouch )
    Printf( "WARNING : There are bytes missing at the end of %s.\n          "
           "The end of the MIDI file might not fit the original one.\n", mus ) ;
  if( !division )
    division = 89 ;
  else
    if( !nodisplay ) Printf( "Ticks per quarter note set to %d.\n", division ) ;
  if( !nodisplay )
    {
      if( division != 89 )
        {
          time = TotalTime / 140 ;
          min = time / 60 ;
          sec = (char) (time - min*60) ;
          Printf( "Playing time of the MUS file : %u'%.2u''.\n", min, sec ) ;
        }
      time = (TotalTime * 89) / (140 * division) ;
      min = time / 60 ;
      sec = (char) (time - min*60) ;
      if( division != 89 )
        Printf( "                    MID file" ) ;
      else
        Printf( "Playing time" ) ;
      Printf( " : %u'%.2u''.\n", min, sec ) ;
    }
  if( !nodisplay )
    {
      Printf( "Writing..." ) ;
      fflush( stdout ) ;
    }
  WriteMIDheader( TrackCnt+1, division, file_mid ) ;
  WriteFirstTrack( file_mid ) ;
  for( i = 0 ; i < (int) TrackCnt ; i++ )
    WriteTrack( i, file_mid, track ) ;
  if( !nodisplay )
    Printf( "done !\n" ) ;
  if( !nodisplay && !nocomp )
    Printf( "Compression : %u%%.\n",
           (100 * n) / (n+ (int4) ftell( file_mid )) ) ;

  FreeTracks( track ) ;
#ifdef MSDOG
  fclose(file_mus);
  fclose(file_mid);
  if( !access( mid, 0 ) )
    if( unlink( mid ) )
      return( CWMIDFILE ) ;

  if( rename( tmp, mid ) )
    return( CWMIDFILE ) ;

#else
  fclose(file_mus);
  fclose(file_mid);
#endif

  return 0 ;
}


int convert( const char *mus, const char *mid, int nodisplay, int div,
            int size, int nocomp, int *ow )
{
  FILE *file ;
  int error;
#ifdef MSDOG
  int n ;
#endif
  struct stat file_data ;
  char buffer[30] ;


  /* we don't need _all_ that checking, do we ? */
  /* Answer : it's more user-friendly */
#ifdef MSDOG

  if( access( mus, 0 ) )
    {
      Printf( "ERROR : %s does not exist.\n", mus ) ;
      return 1 ;
    }

#else
  if ( !*ow ) {
    file = fopen(mid, "r");
    if ( file ) {
      fclose(file);
      Printf( "qmus2mid: file %s exists, not removed.\n", mid ) ;
      return 2 ;
    }
  }
#endif

  error = qmus2mid( mus, mid, nodisplay, div, size, nocomp ) ;

  if( error )
    {
      Printf( "ERROR : " ) ;
      switch( error )
        {
        case NOTMUSFILE :
          Printf( "%s is not a MUS file.\n", mus ) ; break ;
        case COMUSFILE :
          Printf( "Can't open %s for read.\n", mus ) ; break ;
        case COTMPFILE :
          Printf( "Can't open temp file.\n" ) ; break  ;
        case CWMIDFILE :
          Printf( "Can't write %s (?).\n", mid ) ; break ;
        case MUSFILECOR :
          Printf( "%s is corrupted.\n", mus ) ; break ;
        case TOOMCHAN :
          Printf( "%s contains more than 16 channels.\n", mus ) ; break ;
        case MEMALLOC :
          Printf( "Not enough memory.\n" ) ; break ;
        default : break ;
        }
      return 4 ;
    }

  if( !nodisplay )
    {
      Printf( "%s converted successfully.\n", mus ) ;
      if( (file = fopen( mid, "rb" )) != NULL )
        {
          stat( mid, &file_data ) ;
          fclose( file ) ;
          sprintf( buffer, " : %lu bytes", (unsigned long) file_data.st_size ) ;
        }
      Printf( "%s (%scompressed) written%s.\n", mid, nocomp ? "NOT " : "",
             file ? buffer : ""  ) ;
    }

  return 0 ;
}


int CheckParm( char *check, int argc, char *argv[] )
{
  int i;

  for ( i = 1 ; i<argc ; i++ )
#ifdef MSDOG
    if( !stricmp( check, argv[i] ) )
#else
    if( !strcmp( check, argv[i] ) )
#endif
      return i ;

  return 0;
}


void PrintHeader( void )
{
/*  Print( "===============================================================================\n"
         "              Quick MUS->MID v2.0 ! (C) 1995,96 Sebastien Bacquet\n"
         "                        E-mail : bacquet@iie.cnam.fr\n"
         "===============================================================================\n" ) ;*/
}


void PrintSyntax( void )
{
  PrintHeader() ;
  Printf(
#ifdef MSDOG
         "\nSyntax : QMUS2MID musfile1[.mus] {musfile2[.mus] ... | "
         "midifile.mid} [options]\n"
         "   Wildcards are accepted.\n"
         "   Options are :\n"
         "     -query    : Query before processing\n"
         "     -ow       : OK, overwrite (without query)\n"
#else
         "\nSyntax : QMUS2MID musfile midifile [options]\n"
         "   Options are :\n"
#endif
         "     -noow     : Don't overwrite !\n"
         "     -nodisp   : Display nothing ! (except errors)\n"
         "     -nocomp   : Don't compress !\n"
         "     -size ### : Set the track buffer size to ### (in KB). "
         "Default = 64 KB\n"
         "     -t ###    : Ticks per quarter note. Default = 89\n"
         ) ;
}


