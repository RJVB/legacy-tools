/* Simple trigger detection on RS232-CTS
 \ For Silicon Graphics Irix
 \ (C) CNRS/R.J.V. Bertin 1999-2005
 */
 
int wait_for_CTS( char *device, int *FD, Boolean block, Boolean alter )
/*
 \ device: the name of the serial device to use
 \ FD: pointer to the filedescriptor that will reference the opened device
 \ block: whether to do blocking reads, or not
 \ alter: True: wait for the CTS line to change state / False: wait for CTS to go high (a +5V signal).
 */
{ int result, pparm;
  int fd= *FD;
  struct termio termio;
	if( fd== -1 ){
		fd= open( device, O_RDONLY|O_NDELAY );
		if( fd== -1 || errno ){
			fprintf( stderr, "wait_for_CTS(): Can't open serial port %s (%s)\n", device, serror() );
			return(-1);
		}
		result = ioctl( fd, TCGETA, &termio );

		termio.c_oflag = 0;
		termio.c_iflag= 0;
		termio.c_cflag = B19200 | CS8 | CREAD | CLOCAL /* | CNEW_RTSCTS */;
		termio.c_lflag = 0; /* -ECHO */

		ioctl( fd, TCSETA, &termio );
	}

	if( block ){
	  /* On slow, non-real-time machines like an R5000 O2, it is important to maximise the
	   \ likelihood that we return immediately after the change we're waiting for. Thus, even
	   \ if there (may) exist elegant ways to wait for a change (elegant = not hogging the CPU)
	   \ we prefer to busy-wait here. We read the CTS status using an appropriate call to ioctl()
	   \ inside a tight loop. We do check for continue_CTS_wait: this global flag can be set True
	   \ in an interrupt handler and provides an elegant means of exiting from a blocking wait
	   \ without killing the programme.
	   */
		if( alter ){
			ioctl( fd, TIOCMGET, &pparm);
			if( pparm & TIOCM_CTS ){
				while( (pparm & TIOCM_CTS) && continue_CTS_wait ){
					ioctl( fd, TIOCMGET, &pparm);
				}
			}
			else{
				while( !(pparm & TIOCM_CTS) && continue_CTS_wait ){
					ioctl( fd, TIOCMGET, &pparm);
				}
			}
		}
		else{
		  /* A busy wait loop until we find that the CTS line is high	*/
			do{
				ioctl( fd, TIOCMGET, &pparm);
			} while( !(pparm & TIOCM_CTS) && continue_CTS_wait );
		}
		  /* Having detected a change, we can close the device. If a next change has to be detected
		   \ soon, it may be more efficient to leave it open instead. The current device state (open/closed)
		   \ is returned via the FD argument. NB: on Unix, -1 is an invalid filedescriptor, so can be used
		   \ to signal "closed" (0 is stdin).
		   */
		close( fd );
		fd= -1;
		*FD= fd;
		return(1);
	}
	else{
		ioctl( fd, TIOCMGET, &pparm);
		if( pparm & TIOCM_CTS ){
		  /* CTS is high. This is all we need to know for now, so close the device, and return TRUE */
			close( fd );
			fd= -1;
			*FD= fd;
			return( 1 );
		}
		else{
			*FD= fd;
			return(0);
		}
	}
}


int get_CTS( char *device, int *FD )
{ int result, pparm;
  int fd= *FD;
  struct termio termio;
	if( fd== -1 ){
		errno= 0;
		fd= open( device, O_RDONLY|O_NDELAY );
		if( fd== -1 || errno ){
			fprintf( stderr, "get_CTS(): Can't open serial port %s (%s)\n", device, serror() );
			return(-1);
		}
		result = ioctl( fd, TCGETA, &termio );

		termio.c_oflag = 0;
		termio.c_iflag= 0;
		termio.c_cflag = B19200 | CS8 | CREAD | CLOCAL /* | CNEW_RTSCTS */;
		termio.c_lflag = 0; /* -ECHO */

		ioctl( fd, TCSETA, &termio );
	}

	ioctl( fd, TIOCMGET, &pparm);
	close( fd );
	fd= -1;
	*FD= fd;
	if( pparm & TIOCM_CTS ){
		return( 1 );
	}
	else{
		return(0);
	}
}


***********************************************************************************************
/* Reading the CTS line status using the provided routines: */

		{ char c;
			fputc( '\n', stderr );
			fseek( stdin, 0, SEEK_END );
			do{
			  /* Notify the user to position the triggering device and wait for his/her confirmation that
			   \ it is ready to go. Provide feedback about the device's state if requested (this could be
			   \ done if we detect a change...
			   */
				fprintf( stderr, "CTS/triggering device state: %s\n", CTS_states[ get_CTS( wsd, &Wait_FD)+ 1 ] );
				fprintf( stderr, "Setup the CTS/triggering device now, and then\n" );
				fprintf( stderr, "Please hit enter to continue (or <anykey><enter> to verify triggering state)... " );
				fflush( stderr );
				c= getchar();
				if( c!= '\n' ){
					fprintf( stderr, "\nCTS/triggering device state: %s\n",
						CTS_states[ get_CTS( wsd, &Wait_FD)+ 1 ]
					);
					fflush( stderr );
					  /* flush the newline:	*/
					getchar();
				}
			} while( c!= '\n' && continue_CTS_wait );
			fputc( '\n', stderr );
			fprintf( stderr, "CTS/triggering device state: %s\n",
				CTS_states[ get_CTS( wsd, &Wait_FD)+ 1 ]
			);
		}


/* Initiate waiting for CTS: wait for a change in state (high->low or low->high). */
		fprintf( stderr, "Waiting on %s for CTS to switch\n", wait_serial_device );
		fflush( stderr );
		while( !wait_for_CTS(wait_serial_device, &Wait_FD, True, True) && continue_CTS_wait ){
			;
		}

