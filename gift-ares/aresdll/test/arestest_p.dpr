program arestest_p;

uses
  Forms,
  arestest in 'arestest.pas' {MainForm},
  ARESDLL in 'ARESDLL.PAS';

{$R *.res}

begin
  Application.Initialize;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
end.
