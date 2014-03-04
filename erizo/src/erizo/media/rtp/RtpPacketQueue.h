#ifndef __RTPPACKETQUEUE_H__
#define __RTPPACKETQUEUE_H__

#include <map>
#include "logger.h"

namespace erizo{

  typedef std::map< int, unsigned char *> PACKETQUEUE;
  typedef std::map< int, int > LENGTHQ;

  /**
   * Esta clase se encarga de cuando llega un paquete pasarselo al playchannel si es el siguiente en el flujo o
   * encolarlo si no. Los paquetes se mantienen en la cola hasta que llegan los paquetes desordenados o
   * hasta que ha pasado el tiempo máximo en cola.
   */
  class RtpPacketQueue
  {
    public:

      /**
       * Constructor de la clase.
       */
      RtpPacketQueue();

      /**
       * Destructor virtual
       */
      virtual ~RtpPacketQueue(void);

      /**
       * Método para procesar paquetes recibidos.
       */
      void packetReceived(const unsigned char *data, int length);

    private:

      /**
       * Guarda un paquete en la cola de paquetes
       */
      void enqueuePacket(const unsigned char *data, int length, uint16_t nseq);

      /**
       * Comprueba si hay que enviar alguno de los paquetes de la cola
       */
      void checkQueue(void);

      /**
       * Borra la cola
       */
      void cleanQueue(void);

      /**
       * Envia el primer paque de la cola
       */
      void sendFirst(void);

      /**
       * Numero maximo de diferencia entre un paquete y otro.
       * Un numero mayor indicaria reseteo del numero de secuencia.
       **/
      static const int MAX_DIFF = 25;

      /**
       * Numero maximo de diferencia en TS entre un paquete y otro.
       * Un numero mayor indicaria reseteo del numero del TS.
       */
      static const int MAX_DIFF_TS = 5000;

      /**
       * Tamaño de la cola de paquetes
       */
      static const int MAX_SIZE = 10;

      /**
       * cola de paquets
       */
      PACKETQUEUE queue;

      /**
       * cola con las longitudes de los paquetes
       */
      LENGTHQ lqueue;

      /**
       * Ultimo número de secuencia enviado hacia el playchannel
       */
      uint16_t lastNseq;

      /**
       * Timestamp del ultimo paquete recibido.
       */
      uint32_t lastTs;

  };
} /* namespace erizo */

#endif /* RTPPACKETQUEUE*/

