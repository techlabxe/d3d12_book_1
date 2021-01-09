# 説明

このリポジトリは、技術書典６で頒布した
「DirectX12 Programming Vol.1」という本のサンプルプログラムを格納したものです。

# 不具合など

バグや不明点などあれば、本リポジトリの Issue のほうからお問い合わせください。
可能な範囲でサポートの方を行いたいと思います。

# モデルデータについて

ニコニ立体： https://3d.nicovideo.jp/alicia/ で公開されている
「アリシア・ソリッド」のモデルを使用しています。

公開されているデータ種別のうち VRM　モデルを使用しています。
サンプルプログラムで使用するために、VRM モデルを一度 UniVRM を用いて再エクスポートしています。
glTF のバリデーション（検証）に失敗する場合にはこの手が使えるようです。

同様の手順で、バンダイナムコスタジオが公開しているミライ小町モデルも使用可能です。  
バンダイナムコスタジオ ミライ小町： https://www.bandainamcostudios.com/works/miraikomachi/dlcguideline.html

他にも VRoid Studio で生成したキャラクターデータ（VRM)も本サンプルで読み込めます。


# ライセンスについて

本リポジトリで使用しているオープンソースライブラリ以外の部分については、MIT ライセンスとします。  
同梱しているモデルデータの更なる再配布は禁止とさせていただきます。 
各オープンソースライブラリについては配布元のライセンスに従ってください。

# その他情報

- 2021/01 某日 Visual Studio 2019 を用いてビルドできない不具合を修正
    - 書籍のコード情報と違う状態になっていますがご容赦ください

