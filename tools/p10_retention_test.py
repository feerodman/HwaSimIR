import hashlib,json,os,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
import rk3588_retain_versions as r
ROOT=Path(__file__).resolve().parents[1]/'logs/p10/retention_tests';ROOT.mkdir(parents=True,exist_ok=True)
class Retention(unittest.TestCase):
    def test_complete_groups_pins_live_and_receipt(self):
        with tempfile.TemporaryDirectory(dir=ROOT) as temp:
            root=Path(temp).resolve();assert ROOT.resolve() in root.parents
            def package(suffix,text):
                cfg=root/('Config'+suffix);cfg.mkdir();(cfg/'data').write_text(text)
                (cfg/'deployment_manifest.sha256').write_text(r.digest(cfg/'data')+'  data\n')
                d={'ConfigManifestSha256':r.digest(cfg/'deployment_manifest.sha256')}
                for name,key in [('HwaSim_IR','ElfSha256'),('run_precise.sh','LauncherSha256'),('rk3588_hwasimir_performance_mode.sh','PerformanceToolSha256')]:
                    f=root/(name+suffix);f.write_text(name+text);d[key]=r.digest(f)
                (cfg/'deployment_version.env').write_text(''.join(k+'='+v+'\n' for k,v in d.items()))
                return d
            current=package('','current');package('.before_20260101-000001','old');package('.before_20260101-000002','rollback')
            (root/'Config.new_20260101-000003').mkdir();(root/'driver').mkdir();(root/'driver/licence').write_text('never touch')
            plan=r.plan(root,refs=[]);self.assertEqual(plan['rollback']['suffix'],'.before_20260101-000002');self.assertEqual(len(plan['delete']),1)
            (root/'retention_pins.json').write_text(json.dumps({'versions':['20260101-000001']}))
            self.assertEqual(len(r.plan(root,refs=[])['delete']),0);(root/'retention_pins.json').unlink()
            active=r.plan(root,refs=[str(root/'HwaSim_IR.before_20260101-000001')]);self.assertEqual(len(active['delete']),0)
            with self.assertRaises(ValueError):r.execute(root,plan,dict(result='FAIL'))
            # Simulated free-space clock only; real exact-version deletion in a
            # resolved temporary workspace verifies the destructive operation.
            class Space:f_bavail=100;f_frsize=4096
            receipt=dict(result='PASS',tests=['synthetic_fixture'],**current)
            with patch.object(r,'live_references',return_value=[]),patch.object(os,'statvfs',return_value=Space(),create=True),patch.object(os,'sync',create=True):
                result=r.execute(root,plan,receipt)
            self.assertEqual(len(result['deleted']),4);self.assertTrue((root/'Config').exists());self.assertTrue((root/'Config.before_20260101-000002/data').exists())
            self.assertTrue((root/'Config.new_20260101-000003').exists());self.assertEqual((root/'driver/licence').read_text(),'never touch')
            r.version(root,full=True);r.version(root,'.before_20260101-000002',full=True)
if __name__=='__main__':unittest.main()
