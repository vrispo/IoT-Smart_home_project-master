
import json

class IoT_node():
	room  	 = ""
	res   	 = ""
	id    	 = 0
	ip_v6 	 = ""
	uri		 = ""
	op		 = ""
	p_format = ""
	g_format = ""

#############################CONSTRUCTOR#############################

	def __init__(self, room, res, id, ip, uri, op, p_f, g_f):
	    self.room  		= room
	    self.res   		= res		#tipo 
	    self.id    		= id
	    self.ip_v6 		= str(ip)
	    self.uri   		= uri
	    self.op			= op
	    self.p_format 	= p_f
	    self.g_format	= g_f

#############################ACCESSORS###############################

	def set_room(self, room):
		self.room = room

	def set_res(self, res):
		self.res = res

	def set_id(self, id):
		self.id = id

	def set_ip_v6(self, ip):
		self.ip_v6 = ip

	def get_room(self):
		return str(self.room)

	def get_res(self):
		return str(self.res)

	def get_id(self):
		return int(self.id)

	def get_ip_v6(self):
		return str(self.ip_v6)

	def get_uri(self):
			return str(self.uri)

	def get_operation(self):
		return str(self.op)

	def get_p_format(self):
		return int(self.p_format)

	def get_g_format(self):
		return str(self.g_format)

#############################METHODS##################################

	def to_json(self):
	    context = {'room' : self.room, 'res' : self.res, 'id' : self.id, 'ip' : self.ip_v6}
	    return json.dumps(context)


