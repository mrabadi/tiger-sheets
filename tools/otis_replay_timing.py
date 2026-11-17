"""Order read-only source overlays without changing native worksheet actions."""
from copy import deepcopy

def time_value(step):
    return sum(float(value)*60**n for n,value in enumerate(reversed(step['videoTime'].split(':'))))

def merge_timed_overlays(records,overlays):
    result=deepcopy(records)
    def interval(record):
        times=[time_value(record)]
        for child in record['action'].get('steps',[]):
            start,end=interval(child);times.extend([start,end])
        return min(times),max(times)
    def insert(records,overlay):
        at=time_value(overlay)
        for record in records:
            if record['action']['type']!='modal':continue
            start,end=interval(record)
            if start<=at<=end:
                children=record['action']['steps']
                if not insert(children,overlay):
                    # At an observed transition's timestamp the source artwork
                    # state is already visible/absent, before that key action.
                    position=next((i for i,child in enumerate(children) if time_value(child)>=at),len(children))
                    children.insert(position,overlay)
                return True
        return False
    for overlay in overlays:
        overlay=deepcopy(overlay)
        if not insert(result,overlay):result.append(overlay)
    return sorted(result,key=time_value)
