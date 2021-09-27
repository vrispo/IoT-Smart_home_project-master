import asyncio, aiocoap
from aiocoap import *

def obs_call(response):
    print(response.payload)

async def coap_get_with_observer():
    protocol = await Context.create_client_context()
    request = Message(code= GET, uri= 'coap://[fd00::207:7:7:7]/alarm_s', observe=0)     #coap://localhost...

    pr = protocol.request(request)

    r = await pr.response
    print("First response: %s\n%r"%(r, r.payload))
    
    pr.observation.register_callback(obs_call)

event_loop = asyncio.new_event_loop()
asyncio.set_event_loop(event_loop)
event_loop.create_task(coap_get_with_observer())
asyncio.get_event_loop().run_forever()



'''
async def main():
    protocol = await Context.create_client_context()

    request = Message(code=GET, uri='coap://[fd00::207:7:7:7]/alarm_s', observe=0)

    pr = protocol.request(request)

    r = await pr.response
    print("First response: %s\n%r"%(r, r.payload))

    async for r in pr.observation:
        print("Next result: %s\n%r"%(r, r.payload))

        #pr.observation.cancel()
        #break
    print("Loop ended, sticking around")
    #await asyncio.sleep(50)

if __name__ == "__main__":
    asyncio.get_event_loop().run_until_complete(main())
'''