#include "alexsdl.h"
#include "alexether.h"

enum {
	WIDTH = 640,
	HEIGHT = 480,
};

enum {
	SPEED = 100,
};

int port, sock, keyboard_fd, mousebutton[10];
double mouse_x, mouse_y;
char pktbackup[1024], msg[1024];
FILE *fp;

struct hostent *hostlist;
struct sockaddr_in sa;

struct unit {
	struct unit *next;
	double x, y, h, w, center_x, center_y;
	double top, bottom, left, right;
	double mov_x, mov_y, moveto_x, moveto_y, moving, selected;
	double lasttime;
	Uint32 color;
};

struct unit *first_unit, *last_unit;

struct rect {
	double x1, y1, x2, y2;
	int drawing, direction;
	Uint32 color;
};

struct rect selectbox;

struct dest {
	double x, y;
	SDL_Surface *frames[8];
	double lasttime, drawing;
};

struct dest destimg;

struct pathblock {
	struct pathblock *next;
	double x, y, h, w;
	double top, bottom, left, right;
	Uint32 color;
};

struct pathblock *first_pathblock, *last_pathblock;

void
sendpacket (char str[1024], struct sockaddr_in *sa)
{
	if (sendto (sock, str, strlen (str), 0,
                    (struct sockaddr*) sa,
                    sizeof *sa) < 0) {
                fprintf (stderr, "error sending packet\n");
        }
}

void
stream_data (void)
{
	struct unit *up;
	msg[0] = 0;

	for (up = first_unit; up; up = up->next) {
		if (msg[0] == 0) {
			sprintf (msg, "unit stream\n%d, %d, %d, %d",
				 (int) up->x, (int) up->y, (int) up->h,
				 (int) up->w);
		} else {
			sprintf (msg, "%s\n%d, %d, %d, %d", msg, (int) up->x,
				 (int) up->y, (int) up->h, (int) up->w);
		}
	}
	sendpacket (msg, &sa);
}

void
unit_def (struct unit *up, struct pathblock *pp)
{
	if (up) {
		up->top = up->y;
		up->bottom = up->y + up->h;
		up->left = up->x;
		up->right = up->x + up->w;
		up->center_x = up->x + up->w / 2;
		up->center_y = up->y + up->h / 2;
	}
	if (pp) {
		pp->top = pp->y;
		pp->bottom = pp->y + pp->h;
		pp->left = pp->x;
		pp->right = pp->x + pp->w;
	}
}

void
init_pathblock (void)
{
	struct pathblock *pp;
	char c, line[1000];
	int x, y, w, h;

	c = getc (fp);
	
	while (c != EOF) {
		ungetc (c, fp);
		fgets (line, sizeof line, fp);

		pp = xcalloc (1, sizeof *pp);
		if (first_pathblock == NULL) {
			first_pathblock = pp;
		} else {
			last_pathblock->next = pp;
		}

		last_pathblock = pp;

		sscanf (line, "%d, %d, %d, %d", &x, &y, &w, &h);

		pp->w = w;
		pp->h = h;
		pp->x = x;
		pp->y = y;
		pp->color = 0x777777ff;
		unit_def (NULL, pp);

		c = getc (fp);
	}
}

void
init_units (void)
{
	double units;

	for (units = 0; units < 1; units++) {
		struct unit *up;
		up = xcalloc (1, sizeof *up);

		if (first_unit == NULL) {
			first_unit = up;
		} else {
			last_unit->next = up;
		}

		last_unit = up;

		up->w = 40;
		up->h = 20;
		up->x = WIDTH / 2 - up->w / 2;
		up->y = HEIGHT / 2 - up->h / 2;
		up->color = 0x00ff00ff;
		unit_def (up, NULL);
		up->moveto_x = up->center_x;
		up->moveto_y = up->center_y;
		up->lasttime = get_secs();
		up->moving = 0;
	}
}

void
init_selectbox (void)
{
	selectbox.drawing = 0;
	selectbox.color = 0x0000ffff;
	
}

void
init_destimg (void)
{
	double now;

	now = get_secs ();

	load_blit (&destimg.frames[0], "destination/frame00.png");
	load_blit (&destimg.frames[1], "destination/frame01.png");
	load_blit (&destimg.frames[2], "destination/frame02.png");
	load_blit (&destimg.frames[3], "destination/frame03.png");
	load_blit (&destimg.frames[4], "destination/frame04.png");
	load_blit (&destimg.frames[5], "destination/frame05.png");
	load_blit (&destimg.frames[6], "destination/frame06.png");
	load_blit (&destimg.frames[7], "destination/frame07.png");
	destimg.lasttime = now;
}

void
run_inits (void)
{
	init_units ();
	init_selectbox ();
	init_destimg ();
}

void
check_direction (struct rect *sp)
{
	if (sp->x1 > sp->x2) {
		if (sp->y1 > sp->y2) {
			sp->direction = 1;
		} else {
			sp->direction = 3;
		}
	} else {
		if (sp->y1 > sp->y2) {
			sp->direction = 2;
		} else {
			sp->direction = 4;
		}
	}
}

int
select_check (double left, double right, double top, double bottom)
{
	struct unit *up;
	double collide;

	for (up = first_unit; up; up = up->next) {
		if (right < up->left
		    || left > up->right
		    || top > up->bottom
		    || bottom < up->top) {
			up->selected = 0;
		} else {
			up->selected = 1;
		}
	}
	return (collide);
}

void
selecting (void)
{
	struct unit *up;

	if (mousebutton[1]) {
		if (selectbox.drawing == 0) {
			selectbox.drawing = 1;
			selectbox.x1 = mouse_x;
			selectbox.y1 = mouse_y;
			selectbox.x2 = mouse_x;
			selectbox.y2 = mouse_y;
			check_direction (&selectbox);
		} else {
			selectbox.x2 = mouse_x;
			selectbox.y2 = mouse_y;
			check_direction (&selectbox);
		}
		for (up = first_unit; up; up = up->next) {
			switch (selectbox.direction) {
			case 1:
				select_check (selectbox.x2, selectbox.x1,
					      selectbox.y2, selectbox.y1);
				break;
			case 2:
				select_check (selectbox.x1, selectbox.x2,
					      selectbox.y2, selectbox.y1);
				break;
			case 3:
				select_check (selectbox.x2, selectbox.x1,
					      selectbox.y1, selectbox.y2);
				break;
			case 4:
				select_check (selectbox.x1, selectbox.x2,
					      selectbox.y1, selectbox.y2);
				break;
			}
		}
	}
	if (mousebutton[1] == 0) {
		selectbox.drawing = 0;
	}
}

void
destination (void)
{
	double now;
	struct unit *up;
	
	if (mousebutton[3]) {
		for (up = first_unit; up; up = up->next) {
			if (up->selected) {
				now = get_secs();
				
				up->moveto_x = mouse_x;
				up->moveto_y = mouse_y;
				up->lasttime = now;
				up->moving = 1;
			}
		}
		destimg.drawing = 1;
		destimg.x = mouse_x - 20;
		destimg.y = mouse_y - 20;
		destimg.lasttime = now;
	}
}

int
collision_x (struct unit *up1)
{
	struct unit *up2;
	struct pathblock *pp;
	double collide;

	collide = 0;

	if (up1->left + up1->mov_x <= 0 || up1->right + up1->mov_x >= WIDTH) {
		return (1);
	}
	for (up2 = first_unit; up2; up2 = up2->next) {
		if (up2 == up1) {
			continue;
		}
		if (up1->right + up1->mov_x < up2->left
		    || up1->left + up1->mov_x > up2->right
		    || up1->top > up2->bottom
		    || up1->bottom < up2->top) {
			collide = 0;
		} else {
			return (1);
		}
	}
	for (pp = first_pathblock; pp; pp = pp->next) {
		if (up1->right + up1->mov_x < pp->left
		    || up1->left + up1->mov_x > pp->right
		    || up1->top > pp->bottom
		    || up1->bottom < pp->top) {
			collide = 0;
		} else {
			return (1);
		}
	}
	return (collide);
}

int
collision_y (struct unit *up1)
{
	struct unit *up2;
	struct pathblock *pp;
	double collide;

	if (up1->top + up1->mov_y <= 0 || up1->bottom + up1->mov_y >= HEIGHT) {
		return (1);
	}
	for (up2 = first_unit; up2; up2 = up2->next) {
		if (up2 == up1) {
			continue;
		}
		if (up1->top + up1->mov_y > up2->bottom
		    || up1->bottom + up1->mov_y < up2->top
		    || up1->right < up2->left
		    || up1->left > up2->right) {
			collide = 0;
		} else {
			return (1);
		}
	}
	for (pp = first_pathblock; pp; pp = pp->next) {
		if (up1->top + up1->mov_y > pp->bottom
		    || up1->bottom + up1->mov_y < pp->top
		    || up1->right < pp->left
		    || up1->left > pp->right) {
			collide = 0;
		} else {
			return (1);
		}
	}
	return (collide);
}

void
moving (void)
{
	double now, dt, dx, dy, theta;
	struct unit *up;

	for (up = first_unit; up; up = up->next) {
		now = get_secs ();

		if (up->moving) {
			dx = up->moveto_x - up->center_x;
			dy = up->moveto_y - up->center_y;
			theta = atan2 (dy, dx);
			
			dt = now - up->lasttime;
			
			up->mov_x = SPEED * dt * cos (theta);
			up->mov_y = SPEED * dt * sin (theta);
			
			if (fabs (up->mov_x) > fabs (dx)
			    && fabs (up->mov_y) > fabs (dy)) {
				up->x = up->moveto_x - up->w / 2;
				up->y = up->moveto_y - up->h / 2;
				up->moving = 0;
			} else {
				if (collision_x (up)) {
					up->mov_x = 0;
				}
				up->x += up->mov_x;
				if (collision_y (up)) {
					up->mov_y = 0;
				}
				up->y += up->mov_y;
			}
			
			up->lasttime = now;
			unit_def (up, NULL);
		}
	}
}

void
draw (void)
{
	double now, dt;
	struct unit *up;
	struct pathblock *pp;

	now = get_secs ();

	if (destimg.drawing) {
		dt = floor (10 * (now - destimg.lasttime));
		if (dt < 8) {
			switch ((int) dt) {
			case 0:
				draw_blit (destimg.frames[0],
					   destimg.x, destimg.y);
				break;
			case 1:
				draw_blit (destimg.frames[1],
					   destimg.x, destimg.y);
				break;
			case 2:
				draw_blit (destimg.frames[2],
					   destimg.x, destimg.y);
				break;
			case 3:
				draw_blit (destimg.frames[3],
					   destimg.x, destimg.y);
				break;
			case 4:
				draw_blit (destimg.frames[4],
					   destimg.x, destimg.y);
				break;
			case 5:
				draw_blit (destimg.frames[5],
					   destimg.x, destimg.y);
				break;
			case 6:
				draw_blit (destimg.frames[6],
					   destimg.x, destimg.y);
				break;
			case 7:
				draw_blit (destimg.frames[7],
					   destimg.x, destimg.y);
				break;
			}

		} else {
			destimg.drawing = 0;
		}
	}

	for (pp = first_pathblock; pp; pp = pp->next) {
		boxColor (screen, pp->left, pp->top, pp->right, pp->bottom,
			  pp->color);
	}

	for (up = first_unit; up; up = up->next) {
		boxColor (screen, up->left, up->top, up->right, up->bottom,
			  up->color);
		if (up->selected == 1) {
			circleColor (screen, up->center_x, up->center_y,
				     hypot (up->h / 2, up->w / 2) + 3,
				     0x00ff00ff);
			aacircleColor (screen, up->center_x, up->center_y,
				       hypot (up->h / 2, up->w / 2) + 3,
				       0x00ff00ff);
		}
	}

	if (selectbox.drawing) {
		rectangleColor (screen, selectbox.x1, selectbox.y1,
				selectbox.x2, selectbox.y2, selectbox.color);
	}
}

void
process_input (void)
{
	SDL_Event event;
	int key;

	while (SDL_PollEvent (&event)) {
		key = event.key.keysym.sym;
		switch (event.type) {
		case SDL_QUIT:
			exit (0);
		case SDL_KEYUP:
			if (key == SDLK_ESCAPE || key == 'q') {
				exit (0);
			}
		case SDL_MOUSEBUTTONDOWN:
			mousebutton[event.button.button] = 1;
			destination ();
			break;
		case SDL_MOUSEBUTTONUP:
			mousebutton[event.button.button] = 0;
			break;
		case SDL_MOUSEMOTION:
			mouse_x = event.button.x;
			mouse_y = event.button.y;
			break;
		}
	}
}

int
main (int argc, char **argv)
{
	struct timeval tv;
	int n, mode, started_sdl, joined, slen, maxfd;
        char dotted_ip[15], *stringp, *strstep;
	socklen_t sa_len;
	fd_set rset;

	joined = 0;
	mode = 0;
	started_sdl = 0;

	if (argc < 3) {
		printf ("usage: %s host port\n", argv[0]);
		return (1);
	}

        hostlist = gethostbyname (argv[1]);
        if (hostlist == NULL) {
                fprintf (stderr, "unable to resolve %s\n", argv[1]);
                return (1);
        }
        inet_ntop (AF_INET, hostlist->h_addr_list[0], dotted_ip, 15);

        sock = socket (PF_INET, SOCK_DGRAM, IPPROTO_IP);
	keyboard_fd = 0;

        if (sock < 0) {
                fprintf (stderr, "unable to create socket\n");
                exit (1);
        }

        port = atoi (argv[2]);

        memset (&sa, 0, sizeof sa);

        sa.sin_family = AF_INET;

        memcpy (&sa.sin_addr, hostlist->h_addr_list[0], hostlist->h_length);

        sa.sin_port = htons (port);

	sa_len = sizeof sa;

	sendpacket ("join request", &sa);

	if ((n = recvfrom (sock, msg, sizeof msg - 1, 0, (struct sockaddr*)
			   &sa, &sa_len)) > 0) {
		msg[n] = 0;
		slen = strlen (msg) - 1;
		while (slen > 0 && (isspace (msg[slen])) != 0) {
			msg[slen] = 0;
			slen--;
		}
	}

	while (1) {
		FD_ZERO (&rset);
		maxfd = 0;

		FD_SET (sock, &rset);
		if (sock > maxfd) {
			maxfd = sock;
		}

		FD_SET (keyboard_fd, &rset);
		if (keyboard_fd > maxfd) {
			maxfd = keyboard_fd;
		}

		tv.tv_sec = 0;
		tv.tv_usec = 15 * 1000;

		if (select (maxfd + 1, &rset, NULL, NULL, &tv) < 0) {
			fprintf (stderr, "select error\n");
			return (1);
		}

		if (FD_ISSET (sock, &rset)) {
			if ((n = recvfrom (sock, msg, sizeof msg - 1, 0,
					   (struct sockaddr*) &sa, &sa_len))
			    > 0) {
				msg[n] = 0;
				slen = strlen (msg) - 1;
				while (slen > 0
				       && (isspace (msg[slen])) != 0) {
					msg[slen] = 0;
					slen--;
				}
			}
			printf ("received packet: '%s'\n", msg);
			switch (mode) {
			case 0:
				break;
			case 1:
				strcpy (pktbackup, msg);
				strstep = pktbackup;
				if ((stringp = strsep (&strstep, "\n"))
				    != NULL) {
					if (strcmp (stringp, "map name:")
					    == 0) {
						fp = fopen (strstep, "r");
						init_pathblock ();
					}
				}
				break;
			}
		}
		if (FD_ISSET (keyboard_fd, &rset)) {
			fgets (msg, sizeof msg, stdin);
			if (sendto (sock, msg, strlen (msg) + 1, 0,
				    (struct sockaddr*) &sa, sizeof (sa))
			    <= (int) strlen (msg)) {
				fprintf (stderr, "error sending packet\n");
			}
			if (strcasecmp (msg, "quit\n") == 0
			    || strcasecmp (msg, "q\n") == 0) {
				printf ("you have left\n");
			}
			switch (mode) {
			case 0:
				if (strcasecmp (msg, "start game\n") == 0) {
					mode = 1;
					sendpacket ("base map request", &sa);
				}
				break;
			case 1:
				break;
			}
		}
		if (mode == 1) {
			if (started_sdl == 0) {
				alexsdl_init (WIDTH, HEIGHT, SDL_HWSURFACE
					      | SDL_DOUBLEBUF);	
				SDL_WM_SetCaption ("RTS client", NULL);
				run_inits ();
				started_sdl = 1;
			}
			process_input ();
			SDL_FillRect (screen, NULL, 0x000000);
			selecting ();
			moving ();
			stream_data ();
			draw ();
			SDL_Flip (screen);
		}
	}
	return (0);
}
