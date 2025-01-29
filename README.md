C++でFFmpegのlib、dllを利用して動画を読み込み、opencvでフレームを処理し、動画に書き出すサンプルコード

以下の記事のコードを参考に修正した。
https://qiita.com/dugnutt/items/71c1b32cd574d93d972a

dec、encは暫定でhevcのnvidiaを指定。適宜変更ください。
VisualStudio 2022 VC++、ffmpeg7.1で動作確認しました。

