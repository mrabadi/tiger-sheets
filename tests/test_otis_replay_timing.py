import sys
import unittest
from copy import deepcopy
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from otis_replay_timing import merge_timed_overlays

def step(identifier,time,action):
    return {'id':identifier,'videoTime':time,'evidence':{'kind':'visual','eventId':identifier},'action':action}
def keys(identifier,time):return step(identifier,time,{'type':'key_sequence','target':'formatCells.dialog','keys':['Escape']})
def caption(identifier,time):return step(identifier,time,{'type':'video_caption','operation':'show' if identifier=='show' else 'clear'})

class NativeOverlayTiming(unittest.TestCase):
    def test_caption_transitions_execute_inside_actual_modal_children(self):
        original=[keys('before','00:01'),step('dialog','00:03',{'type':'modal','open':{'type':'key_sequence','keys':['Ctrl+1']},'steps':[keys('category','00:04'),keys('type','00:06'),keys('close','00:09')]}),keys('after','00:10')]
        snapshot=deepcopy(original)
        merged=merge_timed_overlays(original,[caption('show','00:05'),caption('clear','00:07')])
        self.assertEqual([s['id'] for s in merged],['before','dialog','after'])
        self.assertEqual([s['id'] for s in merged[1]['action']['steps']],['category','show','type','clear','close'])
        self.assertEqual(original,snapshot,'Scheduling must not rewrite the observed worksheet actions')
    def test_nested_modal_gets_innermost_actual_source_interval(self):
        inner=step('inner','00:05',{'type':'modal','steps':[keys('inner-edit','00:06'),keys('inner-close','00:08')]})
        outer=step('outer','00:02',{'type':'modal','steps':[keys('outer-edit','00:03'),inner,keys('outer-close','00:10')]})
        result=merge_timed_overlays([outer],[caption('show','00:07')])
        self.assertEqual([s['id'] for s in result[0]['action']['steps'][1]['action']['steps']],['inner-edit','show','inner-close'])
    def test_onset_before_modal_remains_visible_into_modal_without_reordering(self):
        modal=step('dialog','09:00',{'type':'modal','steps':[keys('edit','09:02'),keys('close','09:07')]})
        result=merge_timed_overlays([modal],[caption('show','08:59.5'),caption('clear','09:02.5')])
        self.assertEqual([s['id'] for s in result],['show','dialog'])
        self.assertEqual([s['id'] for s in result[1]['action']['steps']],['edit','clear','close'])
    def test_transition_at_close_time_runs_before_actual_close(self):
        modal=step('dialog','00:01',{'type':'modal','steps':[keys('edit','00:02'),keys('close','00:03')]})
        result=merge_timed_overlays([modal],[caption('clear','00:03')])
        self.assertEqual([s['id'] for s in result[0]['action']['steps']],['edit','clear','close'])
if __name__=='__main__':unittest.main()
