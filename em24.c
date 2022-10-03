#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <modbus/modbus.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mosquitto.h>

#define MODBUS_SET_INT32_TO_INT16_REV(tab_int16, index, value) \
    do { \
        tab_int16[(index)    ] = (value); \
        tab_int16[(index) + 1] = (value) >> 16; \
    } while (0)

#define NB_CONNECTION	10

static modbus_t *ctx = NULL;
static modbus_mapping_t *mb_mapping;
static int server_socket = -1;
static int vflag = false;

static void close_sigint(int dummy)
{
    if (server_socket != -1) {
        close(server_socket);
    }
    modbus_free(ctx);
    modbus_mapping_free(mb_mapping);

    exit(dummy);
}

void on_connect(struct mosquitto *mosq, void *obj, int rc) {
	printf("ID: %d\n", * (int *) obj);
	if(rc) {
		printf("Error with result code: %d\n", rc);
		exit(-1);
	}
	mosquitto_subscribe(mosq, NULL, "smartmeter/mains/sensor/#", 0);
}

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
	float u1,i1,p1,e1,n;
	(void)mosq;
	(void)obj;
	if(vflag) {
		printf("New message with topic %s: %s\n", msg->topic, (char *) msg->payload);
	}
	if(strncmp(msg->topic, "smartmeter/mains/sensor/1/obis/1-0:16.7.0/255/value", 51) == 0) {
		sscanf(msg->payload, "%f", &p1);
		u1 = 240.0;
		i1 = p1 / u1;
//		printf("p1=%f, u1=%f, i1=%f\n", p1, u1, i1);
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x0000, (uint16_t)round(u1 * 10));   // u1
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x000c, (uint16_t)round(i1 * 1000)); // i1
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x0012, (uint16_t)round(p1 * 10));   // p1
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x0028, (uint16_t)round(p1 * 10));   // p total
	} else if(strncmp(msg->topic, "smartmeter/mains/sensor/1/obis/1-0:1.8.0/255/value", 50) == 0) {
		sscanf(msg->payload, "%f", &e1);
//		printf("e1=%f\n", e1);
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x0040, (uint16_t)round(e1 * 0.01));   // e1
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x0034, (uint16_t)round(e1 * 0.01));   // e total
	} else if(strncmp(msg->topic, "smartmeter/mains/sensor/1/obis/1-0:2.8.0/255/value", 50) == 0) {
		sscanf(msg->payload, "%f", &n);
//		printf("n=%f\n", n);
		MODBUS_SET_INT32_TO_INT16_REV(mb_mapping->tab_registers, 0x004e, (uint16_t)round(e1 * 0.01));   // e total negative
	}
}

int main(int argc, char *argv[]) {
	uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
	int master_socket;
	int rc, id=12, c;
	fd_set refset;
	fd_set rdset;
	int fdmax;
	int port=502;
	char ip[16] = "0.0.0.0";

	while((c = getopt(argc, argv, "+hi:p:v")) != -1) {
		switch (c) {
		case 'h':
			printf("usage: %s [-h] [-a address] [-p port] [-v]\n", argv[0]);
			printf("-h - help\n");
			printf("-i - ipaddress, default = 0.0.0.0\n");
			printf("-p - port, default = 502\n");
			printf("-v - verbose\n");
			exit(0);
			break;
		case 'i':
			strncpy(ip,optarg,15);
			break;
		case 'p':
			port = atoi(optarg);
			if((port <= 0) || (port > 65535)) {
				printf("Portnumber has to be > 0 and <= 65535\n");
				exit(1);
			}
			break;
		case 'v':
			vflag = true;
			break;
		case '?':
			printf("Use %s -h for help\n", argv[0]);
			exit(1);
			break;
		default:
			break;
		}
	}

	ctx = modbus_new_tcp(ip, port);
	if(ctx == NULL) {
		fprintf(stderr, "Unable to allocate libmodbus context\n");
		return -1;
	}
	if(vflag) {
		modbus_set_debug(ctx, TRUE);
	}

	mb_mapping = modbus_mapping_new(0, 0, 0xFFFF, 0);
	if (mb_mapping == NULL) {
		fprintf(stderr, "Failed to allocate the mapping: %s\n",
                	modbus_strerror(errno));
		modbus_free(ctx);
		return -1;
	}

// https://www.enika.eu/data/files/produkty/energy%20m/CP/em24%20ethernet%20cp.pdf
	mb_mapping->tab_registers[0x000b] = 1650;  // model: EM24DINAV23XE1PFB
	mb_mapping->tab_registers[0xa000] = 7;     // application set to H
	mb_mapping->tab_registers[0x0302] = 0;     // hardware version
	mb_mapping->tab_registers[0x0304] = 0;     // firmware version
	mb_mapping->tab_registers[0x1002] = 3;     // phase config: 1P
	mb_mapping->tab_registers[0x5000] = ('0' << 8) | '0';  // serial
	mb_mapping->tab_registers[0x5001] = ('0' << 8) | '0';  // serial
	mb_mapping->tab_registers[0x5002] = ('0' << 8) | '0';  // serial
	mb_mapping->tab_registers[0x5003] = ('0' << 8) | '0';  // serial
	mb_mapping->tab_registers[0x5004] = ('0' << 8) | '0';  // serial
	mb_mapping->tab_registers[0x5005] = ('0' << 8) | '0';  // serial
	mb_mapping->tab_registers[0x5006] = ('0' << 8);        // serial
	mb_mapping->tab_registers[0xa100] = 0;     // switch position: unlocked kVARh

	mb_mapping->tab_registers[0x0033] = 50 * 10;     // frequency

	mosquitto_lib_init();

	struct mosquitto *mosq;

	mosq = mosquitto_new("em24", true, &id);
	mosquitto_connect_callback_set(mosq, on_connect);
	mosquitto_message_callback_set(mosq, on_message);
	
	rc = mosquitto_connect(mosq, "localhost", 1883, 10);
	if(rc) {
		printf("Could not connect to Broker with return code %d\n", rc);
		modbus_mapping_free(mb_mapping);
		modbus_free(ctx);

		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return -1;
	}

	mosquitto_loop_start(mosq);

	server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
	if(server_socket == -1) {
		fprintf(stderr,"Failed to listen to socket\n");
		modbus_mapping_free(mb_mapping);
		modbus_free(ctx);

		mosquitto_loop_stop(mosq, true);
		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return -1;
	}
/*
	rc = modbus_tcp_accept(ctx, &server_socket);
	if(rc == -1) {
		fprintf(stderr,"Failed to accept new connection\n");
		modbus_mapping_free(mb_mapping);
		modbus_free(ctx);

		mosquitto_loop_stop(mosq, true);
		mosquitto_disconnect(mosq);
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
		return -1;
	}
*/
	signal(SIGINT, close_sigint);

	/* Clear the reference set of socket */
	FD_ZERO(&refset);
	/* Add the server socket */
	FD_SET(server_socket, &refset);

	/* Keep track of the max file descriptor */
	fdmax = server_socket;

	for(;;) {
		rdset = refset;
		if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) {
			perror("Server select() failure.");
			close_sigint(1);
		}

		/* Run through the existing connections looking for data to be read */
		for (master_socket = 0; master_socket <= fdmax; master_socket++) {

			if (!FD_ISSET(master_socket, &rdset)) {
				continue;
			}

			if (master_socket == server_socket) {
				/* A client is asking a new connection */
				socklen_t addrlen;
				struct sockaddr_in clientaddr;
				int newfd;

				/* Handle new connections */
				addrlen = sizeof(clientaddr);
				memset(&clientaddr, 0, sizeof(clientaddr));
				newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
				if (newfd == -1) {
					perror("Server accept() error");
				} else {
					FD_SET(newfd, &refset);

					if (newfd > fdmax) {
					/* Keep track of the maximum */
						fdmax = newfd;
					}
					printf("New connection from %s:%d on socket %d\n",
							inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
				}
			} else {
				modbus_set_socket(ctx, master_socket);
				rc = modbus_receive(ctx, query);
				if (rc > 0) {
					modbus_reply(ctx, query, rc, mb_mapping);
				} else if(rc == -1) {
					/* This server is ended on connection closing or any errors */
					printf("Connection closed on socket %d\n", master_socket);
					close(master_socket);

					/* Remove from reference set */
					FD_CLR(master_socket, &refset);

					if (master_socket == fdmax) {
						fdmax--;
					}
				}
			}
		}
	}
/*
	if(s != -1) {
		close(server_socket);
	}
	modbus_mapping_free(mb_mapping);
	modbus_close(ctx);
	modbus_free(ctx);
	
	mosquitto_loop_stop(mosq, true);
	mosquitto_disconnect(mosq);
	mosquitto_destroy(mosq);
	mosquitto_lib_cleanup();
*/
	return 0;
}
