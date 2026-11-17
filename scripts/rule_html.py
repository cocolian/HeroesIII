from html.parser import HTMLParser
import re
class Node:
 def __init__(self,tag='',attrs=None,parent=None):self.tag=tag;self.attrs=dict(attrs or []);self.children=[];self.parent=parent;self.data=''
 def walk(self,tag=None):
  if tag is None or self.tag==tag:yield self
  for child in self.children:yield from child.walk(tag)
 def text(self):return re.sub(r'\s+',' ',self.data+''.join(c.text()+' ' for c in self.children)).strip()
class Document(HTMLParser):
 def __init__(self,text):super().__init__(convert_charrefs=True);self.root=Node();self.stack=[self.root];self.feed(text)
 def handle_starttag(self,t,a):
  n=Node(t,a,self.stack[-1]);self.stack[-1].children.append(n)
  if t not in ['area','base','br','col','embed','hr','img','input','link','meta','param','source','track','wbr']:self.stack.append(n)
 def handle_endtag(self,t):
  for i in range(len(self.stack)-1,0,-1):
   if self.stack[i].tag==t:self.stack=self.stack[:i];break
 def handle_startendtag(self,t,a):self.handle_starttag(t,a);self.handle_endtag(t)
 def handle_data(self,s):n=Node('#text',parent=self.stack[-1]);n.data=s;self.stack[-1].children.append(n)

def creature_rows(text):
 rows=[]
 for tr in Document(text).root.walk('tr'):
  cells=[n for n in tr.children if n.tag in ('td','th')]
  if len(cells)!=14:continue
  parsed=[{'text':c.text(),'images':[n.attrs.get('alt','')for n in c.walk('img')],'links':[n.attrs.get('title','')for n in c.walk('a')]}for c in cells]
  if parsed[0]['images']:rows.append(parsed)
 return rows
