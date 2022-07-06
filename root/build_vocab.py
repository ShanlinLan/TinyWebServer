"""
    date:2021/6/10
    建立单词表
"""

import json
import pickle
import jieba
import logging
from wordcloud import WordCloud
import PIL.Image as image
import matplotlib.pyplot as plt


class JsonReader(object):

    def __init__(self, json_name, generate_pkl=False):
        """
        @param json_name: json文件的名字
        @param generate_pkl: 是否生成pkl文件
        """

        self.data = self._read_json(json_name)
        self._generate_pkl = generate_pkl

        # 用来生成pkl文件
        if self._generate_pkl:
            self.keys = list(self.data.keys())
        self.items = list(self.data.items())

    def __getitem__(self, item):
        if self._generate_pkl:
            data = self.data[self.keys[item]]
        else:
            data = self.items[item]
        return data

    def __len__(self):
        return len(self.data)

    def _read_json(self, json_name):
        """
        读取json文件
        @param json_name: json文件名称
        @return: json文件内容
        """
        with open(json_name, 'r', encoding='utf-8') as file:
            data = json.load(file)
        return data


class Vocabulary(object):

    def __init__(self):
        self.word2idx = {}  # {word: id}
        self.id2word = {}   # {id: word}
        self.idx = 0
        self.add_word('<pad>')
        self.add_word('<start>')    # 开始标识
        self.add_word('<end>')      # 结束标识
        self.add_word('<unk>')

    def __call__(self, word):
        if word not in self.word2idx:
            return self.word2idx['<unk>']
        return self.word2idx[word]

    def __len__(self):
        return len(self.word2idx)

    def add_word(self, word):
        """
        向单词表中添加单词
        @param word: 待添加的单词
        @return:
        """
        if word not in self.word2idx:
            self.word2idx[word] = self.idx
            self.id2word[self.idx] = word
            self.idx += 1

    def get_word_by_id(self, id):
        return self.id2word[id]


def build_vocab(json_name, generate_pkl=False):
    """
    建立单词表
    @param json_name: json文件名称
    @param generate_pkl: 是否生成pkl文件
    @return: Vocabulary对象
    """
    json_reader = JsonReader(json_name, generate_pkl)

    words = {}
    jieba.setLogLevel(logging.ERROR)
    jieba.load_userdict("user_vocab_dict.txt")

    for items in json_reader.data:
        # Todo: 根据实际文件格式进行修改
        finding = json_reader.data[items].get("finding").strip("。")
        # impression = json_reader.data[items].get("impression").strip("。")

        # 计算单词的词频
        for word in jieba.cut(finding, cut_all=False):
            if word in words:
                words[word] = words[word] + 1
            else:
                words[word] = 1

        # for word in jieba.cut(impression, cut_all=False):
        #     if word in words:
        #         words[word] = words[word] + 1
        #     else:
        #         words[word] = 1

    # 根据词频对单词进行排序
    sorted_words = sorted(words.items(), key=lambda x: x[1], reverse=True)


    word_cloud = {}
    num = 0

    # 将分词结果写入文件中
    print("分词结果共" + str(len(sorted_words)))
    with open("vocab_dict.txt", "w", encoding='utf-8',) as f:
        for word in sorted_words:
            if word[1] >= 3:
                print(word[0] + " : " + str(word[1]))
                f.write(word[0])
                f.write('\n')
                num += word[1]
                word_cloud[word[0]] = word[1]

    for word in word_cloud:
        word_cloud[word] = word_cloud[word] / num

    cloud_image = WordCloud(background_color="white", font_path="MSYHMONO.ttf").generate_from_frequencies(word_cloud)
    plt.imshow(cloud_image)
    plt.show()

    vocab = Vocabulary()

    # TODO:可对不理想的分词进行筛选
    # 将分词结果写入到词典对象中
    for word in sorted_words:
        if word[1] >= 3:
            vocab.add_word(word[0])
            print(word[0])
    return vocab


if __name__ == '__main__':
    json_name = "MVFRG_pat_img_impress_find.json"
    vocab_name = "MVFRG_finding_vocab.pkl"
    generate_pkl = True

    vocab = build_vocab(json_name, generate_pkl)
    with open(vocab_name, 'wb') as f:
        pickle.dump(vocab, f)

    print("Total vocabulary size:{}".format(len(vocab)))
    print("Saved vocabulary in {}".format(vocab_name))
