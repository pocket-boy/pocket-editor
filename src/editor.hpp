//
// Created by root on 3/25/26.
//

#ifndef POCKET_EDITOR_EDITOR_H
#define POCKET_EDITOR_EDITOR_H

class FileNode {
    public:
        std::string path;
        std::string name;
        std::vector<FileNode*> nodes;
        bool isDir;
        bool isExpanded;
};
#endif //POCKET_EDITOR_EDITOR_H