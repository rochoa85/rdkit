import RDConfig
from Dbase import DbModule
from Dbase.DbConnection import DbConnect
import cPickle
import Chem

if RDConfig.usePgSQL:
  dbName="::RDTests"
else:
  dbName="data.sqlt"

molTblName =  'simple_mols1'
fpTblName = 'simple_mols1_fp'
conn = DbConnect(dbName,molTblName)
conn.AddTable(fpTblName,'id varchar(10),autofragmentfp %s'%DbModule.binaryTypeName)
d = conn.GetData()
for smi,id in d:
  print repr(id),repr(smi)
  mol = Chem.MolFromSmiles(smi)
  fp = Chem.RDKFingerprint(mol)
  pkl = cPickle.dumps(fp)
  conn.InsertData(fpTblName,(id,DbModule.binaryHolder(pkl)))
conn.Commit()
