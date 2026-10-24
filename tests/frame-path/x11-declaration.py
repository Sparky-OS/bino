#!/usr/bin/env python3
"""Inspect the live native child contract independently of Bino's GL readback."""
import json,os
from pathlib import Path
from Xlib import X,Xatom,display
c=display.Display();root=c.screen().root
atom=lambda s:c.intern_atom(s)
prop=lambda w,n:w.get_full_property(atom(n),X.AnyPropertyType)
support=prop(root,'_KDE_NET_WM_STEREO_CONTENT_SUPPORTED')
assert support is not None and list(support.value)==[8]
found=[]
for parent in root.query_tree().children:
    children=prop(parent,'_KDE_NET_WM_STEREO_CONTENT_CHILDREN')
    if children is None:continue
    for childid in children.value:
        child=c.create_resource_object('window',int(childid));geometry=child.get_geometry()
        content=prop(child,'_KDE_NET_WM_STEREO_CONTENT')
        place=prop(child,'_KDE_NET_WM_STEREO_CONTENT_CHILD_PLACE')
        assert content is not None and list(content.value)==[3]
        assert place is not None and place.property_type==Xatom.CARDINAL and place.format==32
        assert list(place.value)==[0,0,640,360,1],list(place.value)
        assert (geometry.width,geometry.height)==(640,180)
        parent_content=prop(parent,'_KDE_NET_WM_STEREO_CONTENT')
        assert parent_content is None or list(parent_content.value)==[0]
        found.append({'child':int(childid),'native_size':[geometry.width,geometry.height],'placement':list(map(int,place.value)),'content':3,'parent_content':0})
        if os.environ.get('PIXEL_DROP_DECLARATION'):
            child.delete_property(atom('_KDE_NET_WM_STEREO_CONTENT'))
assert len(found)==1,found
c.sync()
Path(os.environ['CASE_DIR'],'declaration.json').write_text(json.dumps(found,indent=2)+'\n')
