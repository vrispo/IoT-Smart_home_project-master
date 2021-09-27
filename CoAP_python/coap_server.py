
import threading
from threading import Thread, Lock, Condition
import datetime
import asyncio
import aiocoap
import aiocoap.resource as resource
import asyncio
import socket
import sys
from aiocoap import *
import threading

#miei moduli
from application_signals import *
import iot_node
from iot_node import *


RECEIVE_BUF_LEN = 1024
tcp_kill_signal = True




def thread_body(server):
	global tcp_kill_signal
	mutex = Lock()

	# Listen for incoming connections
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)   #connessione di tipo stream e quindi TCP --> reliable transmission
	server_address = ('localhost', 10001)                      #bind sulla porta dedicata al server ovvero 10001 sul loopback address
	sock.settimeout(0.5)                                       #serve per non rimanere bloccato quando Js non fa richieste e il server deve essere terminato
	sock.bind(server_address)                                  #bind con l'indirizzo di localhost
	sock.listen(1)                                             #metto il server in ascolto su connessioni in entrata

	# Wait for a connection
	print('Thread Up and waiting for a connection')

	# Receive the data in small chunks and retransmit it
	while(tcp_kill_signal):
		try:                                                   #il try serve per sollevare l'eccezione sul timeout che evita il deadlock sulla waiting conn
			connection, client_address = sock.accept()         #bloccante ma con il tempo di timemout, si va direttamente nella except e quindi di nuovo qui

			data = connection.recv(RECEIVE_BUF_LEN).decode('ascii')
			print(data)
			query_data = json.loads(data)
			room       = query_data['room']
			resource   = query_data['resource']

			mutex.acquire()
			query_res = server.find_node(room, resource)
			mutex.release()

			res_dict = {}

			for i in range(0,len(query_res)):
				res_dict['ip_'+str(i)]   = query_res[i][0]		#[0] prende il primo elemento della tupla (ip)
				res_dict['id_'+str(i)]  = query_res[i][1]	    #[1] prende il secondo della tupla (id)

			res_dict['sens_nr'] = len(query_res)				#restituisco anche il numero di quei sensori presenti in quella stanza

			res_dict = json.dumps(res_dict).encode('ascii')
			connection.sendall(res_dict)

		except socket.timeout:
			pass                                                #gestione interna, permette periodicamente di testare la condizione di uscita evitando il deadlock


	connection.close()
	print("Succesfully terminated TCP Server thread")
	return






#risorsa che supporta GET PUT e POST
class BlockResource(resource.Resource):
	content = ""
	nodes_list = []

	def __init__(self, uri, node_list, server):
	    super().__init__()
	    self.content = uri                              #stringa che identifica la richiesta
	    self.nodes_list = node_list
	    self.server = server                     #lista dei nodi noti al server



	def find_node(self, query_selector):
	    #bisogna sostituire l'id perchè dà problemi, meglio inviare l'indirizzo ip del nodo che fa la richiesta

	    room       = query_selector["room"]
	    node_type  = query_selector["type"]
	    node_ip    = query_selector["ip"]


	    ip = CANNOT_FIND_NODE

	    if(room != NO_ROOM and node_type != NO_TYPE):
	        for el in self.nodes_list:
	            if(el.get_room() == room and el.get_res() == node_type and el.get_ip_v6() != node_ip): #và levato l'id, fare controllo con ip
	                ip += str(el.get_ip_v6())
	                ip += ','

	    elif(room != NO_ROOM and node_type == NO_TYPE):
	        for el in self.nodes_list:
	            if(el.get_room() == room and el.get_ip_v6() != node_ip):# and el.get_id() != node_id): #và levato l'id, fare controllo con ip
	                ip += str(el.get_ip_v6())
	                ip += ','

	    elif(room == NO_ROOM and node_type != NO_TYPE):
	        for el in self.nodes_list:
	            if(el.get_res() == node_type and el.get_ip_v6() != node_ip):# and el.get_id() != node_id): #và levato l'id, fare controllo con ip
	                ip += str(el.get_ip_v6())
	                ip += ','

	    return ip[:len(ip)-1]


	async def render_get(self, request):
	    requestPayload = request.payload.decode('ascii')
	    print('GET payload: ', requestPayload)

	    node_json = json.loads(requestPayload)
	    ip_v6 = self.find_node(node_json)

	    if(ip_v6 == ""):
	        response_payload = json.dumps("{'r_code':'" + str(GET_SUCCESS) + "','ip':'" + NOT_FOUND + "'}").encode('ascii')
	        return aiocoap.Message(payload = response_payload)

	    ip_v6 = '"'+ ip_v6[1:len(ip_v6)]+ '"'
	    ip_v6 = str(ip_v6)
	    data = '{"r_code":"' + str(GET_SUCCESS) + '","ip":' + ip_v6 +"}"

	    data = data.replace("'", "\\\'")
	    data = data.replace('"', "'")
	    response_payload = json.dumps(data).encode('ascii')

	    return aiocoap.Message(payload = response_payload)


	async def render_put(self, request):
		print('PUT payload: ', request.payload.decode('ascii'))
		node_json = request.payload

		response_payload = str(PUT_SUCCESS).encode('ascii')
		return aiocoap.Message(code= aiocoap.CHANGED, payload=response_payload)


	async def render_post(self, request):
		print('POST payload: ', request.payload.decode('ascii'))
		node_json = json.loads(request.payload.decode('ascii'))

		for el in self.nodes_list:
			if(str(el.get_ip_v6()) == str(node_json["ip"])):
				response_payload = json.dumps("{'r_code':'" + str(POST_SUCCESS) + "', 'room': '" + str(el.get_room()) + "', 'id': " + str(el.get_id()) +"'}").encode('ascii')
				return aiocoap.Message(code=aiocoap.CHANGED, payload=response_payload)

	############################### TCP COMMUNICATION GUI##################################
		try:
	        # Send data
			message = '{ "ip" : "' + str(node_json["ip"]) +'" , "type" : "' + node_json["type"] + '"}'
			sock.sendall(message.encode('ascii'))

	        # Look for the response
			amount_received = 0
			amount_expected = len(message)

			while amount_received < amount_expected:
				data = sock.recv(RECEIVE_BUF_LEN).decode('ascii')
				amount_received += len(data)
		finally:
	        #sock.close()
			pass
		response = json.loads(str(data))
		room          = response["room"]

	#######################################################################################

		id = 0

		for el in self.nodes_list:
			if(el.get_room() == room and el.get_res() == node_json["type"]):
				id = el.id
		id = id + 1

		new_node = IoT_node(room, node_json["type"], id, node_json["ip"], node_json["uri"], node_json["operations"], node_json["post_format"], node_json["get_format"])
		self.server.add_new_node(new_node)
		self.server.add_room(room)


		response_payload = json.dumps("{'r_code':'" + str(POST_SUCCESS) + "', 'room': '" + str(room) + "', 'id': '" + str(id) +"'}").encode('ascii')
		return aiocoap.Message(code= aiocoap.CHANGED, payload=response_payload)





class CoAP_Server():
	nodes_list = []
	rooms_list = []
	root       = 0

	#############################CONSTRUCTOR#############################

	def __init__(self):
		self.nodes_list = []
		self.rooms_list = []
		self.root       = 0
		self.add_root_resource("root")
		print("Server inizializzato correttamente\n")

		# self.nodes_list.append(IoT_node("Bathroom", "washing machine", 1, "127.0.0;1", "/washing_m", "POST","", ""))
		# self.nodes_list.append(IoT_node("Living room", "light sensor", 1, "127.0.0;2", "/res", "POST","", ""))
		# self.nodes_list.append(IoT_node("Bathroom", "light sensor", 1, "127.0.0;4", "/res", "POST","", ""))
		# self.nodes_list.append(IoT_node("Bathroom", "washing machine", 1, "127.0.0.8", "/washing_m", "POST","", ""))


		self.run_server()


	################################METHODS##############################

	def add_new_node(self, node):
		if(node not in self.nodes_list):
			self.nodes_list.append(node)


	def add_room(self, room):
		if(room not in self.rooms_list):
			self.rooms_list.append(room)


	def add_root_resource(self, uri):
		self.root = resource.Site()
		self.root.add_resource(['registration'], BlockResource('registration', self.nodes_list, self))
		self.root.add_resource(['request'], BlockResource('request', self.nodes_list, self))



	#################################################################################
	def find_node(self, room, resource):
		found_list = []

		if(room != NO_ROOM and resource != NO_TYPE):
			for el in self.nodes_list:
				if(el.get_room() == room and el.get_res() == resource):
					found_list.append( (el.get_ip_v6(), el.get_id()) )

		elif(room != NO_ROOM and resource == NO_TYPE):
		    for el in self.nodes_list:
		        if(el.get_room() == room):
		            found_list.append( (el.get_ip_v6(), el.get_id()) )

		elif(room == NO_ROOM and resource != NO_TYPE):
		    for el in self.nodes_list:
		        if(el.get_res() == resource):
		            found_list.append( (el.get_ip_v6(), el.get_id()) )

		return found_list
	#################################################################################



	def run_server(self):
		tcp_server = Thread(target= thread_body ,args=(self,))
		tcp_server.start()

		asyncio.Task(aiocoap.Context.create_server_context(self.root))
		asyncio.get_event_loop().run_forever()


	def reset(self):
		self.nodes_list = []
		self.rooms_list = []









if __name__ == "__main__":
	try:
	    # Create a TCP/IP socket
		sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		# Connect the socket to the port where the server is listening
		server_address = ('localhost', 10000)
		print('connecting to ', server_address,' port ', server_address)
		sock.connect(server_address)
		server = CoAP_Server()

	except(KeyboardInterrupt):
		sock.close()
		tcp_kill_signal = False
		print("\n\nBackend servers succesfully closed and switched to: OFFLINE")
