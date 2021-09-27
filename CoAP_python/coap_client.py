
import asyncio
from aiocoap import *


class CoAP_Client():
    CODE = 0

    async def do_communication(self, request_type, data_from, payload_data, *args, **kwargs):
        protocol = await Context.create_client_context()

        if(request_type == "GET" or request_type == "get"):
            requested_res = Message(code= GET, payload=payload_data.encode('ascii'), uri= data_from)     #coap://localhost...

            try:
                response = await protocol.request(requested_res).response   #aspetto la risposta
            except Exception as e:
                print('Cannot fetch data from: ', data_from)
                print(e)
            else:
                if(str(response.code).split(" ")[self.CODE] == "2.05"):     #il codice 2.05 è definito dallo std CoAP ed è equivalente al codice 200 in HTTP
                    print('Result: ', response.payload.decode('ascii'))     #l'encode deve essere esplicitato in 'ascii' perchè di base è utf-8
        
        
        if(request_type == "POST" or request_type == "post" or request_type == "PUT" or request_type == "put"):
            context = await Context.create_client_context()

            payload = str(payload_data).encode('ascii')
            request = Message(code= POST, payload= payload, uri= data_from)

            response = await context.request(request).response

            print('Result: ', response.code,' | ', response.payload) 



    def run(self, request_type, data_from, payload_data, *args, **kwargs):
        asyncio.get_event_loop().run_until_complete(self.do_communication(request_type, data_from, payload_data)) #esegue in modo asincrono non bloccante




if __name__ == "__main__":
    client = CoAP_Client()

    while(True):
        try:
            ip, req_t = input("IP TYPE: ").split(" ")
            res = input("RES: ")
            payload = input("PAYLOAD: ")
            uri = 'coap://[' + ip + ']/' + res
            client.run(request_type= req_t, data_from=uri, payload_data=payload)
        except KeyboardInterrupt():
            exit()
    
