using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using UnityEngine;
using UnityEngine.Events;
using UnityEngine.Windows.Speech;

[System.Serializable]
public class NewVoiceCommandEvent : UnityEvent<string>
{
}

public class VoiceCommandCenter : MonoBehaviour
{
    public ConfidenceLevel Confidence = ConfidenceLevel.Medium;

    public List<string> Keywords = new List<string>();

    public NewVoiceCommandEvent OnPhraseRecognized = new NewVoiceCommandEvent();

    private KeywordRecognizer keywordRecognizer = null;

    private bool recognizerStarted = false;

    private bool restarting = false;

    private void OnEnable()
    {
        StartRecognizer();
    }

    private void OnDisable()
    {
        StopRecognizer();
    }

    public void AddCommand(string keyword)
    {
        string toLower = keyword.ToLower();

        if (!Keywords.Any(x => x.ToLower().Equals((toLower))))
        {
            Keywords.Add(toLower);
        }

        _ = RestartRecognizer();
    }

    public void RemoveCommand(string keyword)
    {
        string toLower = keyword.ToLower();

        if (Keywords.Any(x => x.ToLower().Equals((toLower))))
        {
            Keywords.Remove(toLower);
        }

        _ = RestartRecognizer();
    }

    protected void KeywordRecognizer_OnPhraseRecognized(PhraseRecognizedEventArgs args)
    {
        if (args.confidence <= Confidence)
        {
            OnPhraseRecognized.Invoke(args.text);
        }
    }

    private void StartRecognizer()
    {
        if (recognizerStarted || Keywords.Count == 0)
        {
            return;
        }
        recognizerStarted = true;

        keywordRecognizer = new KeywordRecognizer(Keywords.ToArray());
        keywordRecognizer.OnPhraseRecognized += KeywordRecognizer_OnPhraseRecognized;
        keywordRecognizer.Start();
    }

    private void StopRecognizer()
    {
        if (!recognizerStarted)
        {
            return;
        }
        recognizerStarted = false;

        keywordRecognizer.OnPhraseRecognized -= KeywordRecognizer_OnPhraseRecognized;
        keywordRecognizer.Stop();

    }

    private async Task RestartRecognizer()
    {
        if (!recognizerStarted || restarting)
        {
            return;
        }
        restarting = true;

        StopRecognizer();

        await Task.Delay(500);

        StartRecognizer();

        restarting = false;
    }
}
